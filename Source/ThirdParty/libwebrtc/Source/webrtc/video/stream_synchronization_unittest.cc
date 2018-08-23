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

#include <algorithm>

#include "test/gtest.h"
#include "video/stream_synchronization.h"

namespace webrtc {

// These correspond to the same constants defined in vie_sync_module.cc.
enum { kMaxVideoDiffMs = 80 };
enum { kMaxAudioDiffMs = 80 };
enum { kMaxDelay = 1500 };

// Test constants.
enum { kDefaultAudioFrequency = 8000 };
enum { kDefaultVideoFrequency = 90000 };
const double kNtpFracPerMs = 4.294967296E6;
static const int kSmoothingFilter = 4 * 2;

class Time {
 public:
  explicit Time(int64_t offset)
      : kNtpJan1970(2208988800UL), time_now_ms_(offset) {}

  NtpTime GetNowNtp() const {
    uint32_t ntp_secs = time_now_ms_ / 1000 + kNtpJan1970;
    int64_t remainder_ms = time_now_ms_ % 1000;
    uint32_t ntp_frac = static_cast<uint32_t>(
        static_cast<double>(remainder_ms) * kNtpFracPerMs + 0.5);
    return NtpTime(ntp_secs, ntp_frac);
  }

  uint32_t GetNowRtp(int frequency, uint32_t offset) const {
    return frequency * time_now_ms_ / 1000 + offset;
  }

  void IncreaseTimeMs(int64_t inc) { time_now_ms_ += inc; }

  int64_t time_now_ms() const { return time_now_ms_; }

 private:
  // January 1970, in NTP seconds.
  const uint32_t kNtpJan1970;
  int64_t time_now_ms_;
};

class StreamSynchronizationTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    sync_ = new StreamSynchronization(0, 0);
    send_time_ = new Time(kSendTimeOffsetMs);
    receive_time_ = new Time(kReceiveTimeOffsetMs);
    audio_clock_drift_ = 1.0;
    video_clock_drift_ = 1.0;
  }

  virtual void TearDown() {
    delete sync_;
    delete send_time_;
    delete receive_time_;
  }

  // Generates the necessary RTCP measurements and RTP timestamps and computes
  // the audio and video delays needed to get the two streams in sync.
  // |audio_delay_ms| and |video_delay_ms| are the number of milliseconds after
  // capture which the frames are rendered.
  // |current_audio_delay_ms| is the number of milliseconds which audio is
  // currently being delayed by the receiver.
  bool DelayedStreams(int audio_delay_ms,
                      int video_delay_ms,
                      int current_audio_delay_ms,
                      int* extra_audio_delay_ms,
                      int* total_video_delay_ms) {
    int audio_frequency =
        static_cast<int>(kDefaultAudioFrequency * audio_clock_drift_ + 0.5);
    int audio_offset = 0;
    int video_frequency =
        static_cast<int>(kDefaultVideoFrequency * video_clock_drift_ + 0.5);
    bool new_sr;
    int video_offset = 0;
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    // Generate NTP/RTP timestamp pair for both streams corresponding to RTCP.
    NtpTime ntp_time = send_time_->GetNowNtp();
    uint32_t rtp_timestamp =
        send_time_->GetNowRtp(audio_frequency, audio_offset);
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    send_time_->IncreaseTimeMs(100);
    receive_time_->IncreaseTimeMs(100);
    ntp_time = send_time_->GetNowNtp();
    rtp_timestamp = send_time_->GetNowRtp(video_frequency, video_offset);
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    send_time_->IncreaseTimeMs(900);
    receive_time_->IncreaseTimeMs(900);
    ntp_time = send_time_->GetNowNtp();
    rtp_timestamp = send_time_->GetNowRtp(audio_frequency, audio_offset);
    EXPECT_TRUE(audio.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));
    send_time_->IncreaseTimeMs(100);
    receive_time_->IncreaseTimeMs(100);
    ntp_time = send_time_->GetNowNtp();
    rtp_timestamp = send_time_->GetNowRtp(video_frequency, video_offset);
    EXPECT_TRUE(video.rtp_to_ntp.UpdateMeasurements(
        ntp_time.seconds(), ntp_time.fractions(), rtp_timestamp, &new_sr));

    send_time_->IncreaseTimeMs(900);
    receive_time_->IncreaseTimeMs(900);

    // Capture an audio and a video frame at the same time.
    audio.latest_timestamp =
        send_time_->GetNowRtp(audio_frequency, audio_offset);
    video.latest_timestamp =
        send_time_->GetNowRtp(video_frequency, video_offset);

    if (audio_delay_ms > video_delay_ms) {
      // Audio later than video.
      receive_time_->IncreaseTimeMs(video_delay_ms);
      video.latest_receive_time_ms = receive_time_->time_now_ms();
      receive_time_->IncreaseTimeMs(audio_delay_ms - video_delay_ms);
      audio.latest_receive_time_ms = receive_time_->time_now_ms();
    } else {
      // Video later than audio.
      receive_time_->IncreaseTimeMs(audio_delay_ms);
      audio.latest_receive_time_ms = receive_time_->time_now_ms();
      receive_time_->IncreaseTimeMs(video_delay_ms - audio_delay_ms);
      video.latest_receive_time_ms = receive_time_->time_now_ms();
    }
    int relative_delay_ms;
    StreamSynchronization::ComputeRelativeDelay(audio, video,
                                                &relative_delay_ms);
    EXPECT_EQ(video_delay_ms - audio_delay_ms, relative_delay_ms);
    return sync_->ComputeDelays(relative_delay_ms, current_audio_delay_ms,
                                extra_audio_delay_ms, total_video_delay_ms);
  }

  // Simulate audio playback 300 ms after capture and video rendering 100 ms
  // after capture. Verify that the correct extra delays are calculated for
  // audio and video, and that they change correctly when we simulate that
  // NetEQ or the VCM adds more delay to the streams.
  // TODO(holmer): This is currently wrong! We should simply change
  // audio_delay_ms or video_delay_ms since those now include VCM and NetEQ
  // delays.
  void BothDelayedAudioLaterTest(int base_target_delay) {
    int current_audio_delay_ms = base_target_delay;
    int audio_delay_ms = base_target_delay + 300;
    int video_delay_ms = base_target_delay + 100;
    int extra_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay;
    int filtered_move = (audio_delay_ms - video_delay_ms) / kSmoothingFilter;
    const int kNeteqDelayIncrease = 50;
    const int kNeteqDelayDecrease = 10;

    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;

    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(1000 -
                                  std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + 2 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;

    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(1000 -
                                  std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay + 3 * filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);

    // Simulate that NetEQ introduces some audio delay.
    current_audio_delay_ms = base_target_delay + kNeteqDelayIncrease;
    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(1000 -
                                  std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    filtered_move = 3 * filtered_move +
                    (kNeteqDelayIncrease + audio_delay_ms - video_delay_ms) /
                        kSmoothingFilter;
    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);

    // Simulate that NetEQ reduces its delay.
    current_audio_delay_ms = base_target_delay + kNeteqDelayDecrease;
    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(1000 -
                                  std::max(audio_delay_ms, video_delay_ms));
    // Simulate base_target_delay minimum delay in the VCM.
    total_video_delay_ms = base_target_delay;
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));

    filtered_move = filtered_move +
                    (kNeteqDelayDecrease + audio_delay_ms - video_delay_ms) /
                        kSmoothingFilter;

    EXPECT_EQ(base_target_delay + filtered_move, total_video_delay_ms);
    EXPECT_EQ(base_target_delay, extra_audio_delay_ms);
  }

  void BothDelayedVideoLaterTest(int base_target_delay) {
    int current_audio_delay_ms = base_target_delay;
    int audio_delay_ms = base_target_delay + 100;
    int video_delay_ms = base_target_delay + 300;
    int extra_audio_delay_ms = 0;
    int total_video_delay_ms = base_target_delay;

    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than this in 1 second.
    EXPECT_GE(base_target_delay + kMaxAudioDiffMs, extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;
    int current_extra_delay_ms = extra_audio_delay_ms;

    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_audio_delay_ms = extra_audio_delay_ms;
    current_extra_delay_ms = extra_audio_delay_ms;

    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_extra_delay_ms = extra_audio_delay_ms;

    // Simulate that NetEQ for some reason reduced the delay.
    current_audio_delay_ms = base_target_delay + 10;
    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // Since we only can ask NetEQ for a certain amount of extra delay, and
    // we only measure the total NetEQ delay, we will ask for additional delay
    // here to try to stay in sync.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
    current_extra_delay_ms = extra_audio_delay_ms;

    // Simulate that NetEQ for some reason significantly increased the delay.
    current_audio_delay_ms = base_target_delay + 350;
    send_time_->IncreaseTimeMs(1000);
    receive_time_->IncreaseTimeMs(800);
    EXPECT_TRUE(DelayedStreams(audio_delay_ms, video_delay_ms,
                               current_audio_delay_ms, &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(base_target_delay, total_video_delay_ms);
    // The audio delay is not allowed to change more than the half of the
    // required change in delay.
    EXPECT_EQ(current_extra_delay_ms +
                  MaxAudioDelayIncrease(
                      current_audio_delay_ms,
                      base_target_delay + video_delay_ms - audio_delay_ms),
              extra_audio_delay_ms);
  }

  int MaxAudioDelayIncrease(int current_audio_delay_ms, int delay_ms) {
    return std::min((delay_ms - current_audio_delay_ms) / kSmoothingFilter,
                    static_cast<int>(kMaxAudioDiffMs));
  }

  int MaxAudioDelayDecrease(int current_audio_delay_ms, int delay_ms) {
    return std::max((delay_ms - current_audio_delay_ms) / kSmoothingFilter,
                    -kMaxAudioDiffMs);
  }

  enum { kSendTimeOffsetMs = 98765 };
  enum { kReceiveTimeOffsetMs = 43210 };

  StreamSynchronization* sync_;
  Time* send_time_;     // The simulated clock at the sender.
  Time* receive_time_;  // The simulated clock at the receiver.
  double audio_clock_drift_;
  double video_clock_drift_;
};

TEST_F(StreamSynchronizationTest, NoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_FALSE(DelayedStreams(0, 0, current_audio_delay_ms,
                              &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(0, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, VideoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(delay_ms / kSmoothingFilter, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(2 * delay_ms / kSmoothingFilter, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(3 * delay_ms / kSmoothingFilter, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, AudioDelay) {
  int current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(delay_ms / kSmoothingFilter, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  int current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason reduced the delay.
  current_audio_delay_ms = 10;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // Since we only can ask NetEQ for a certain amount of extra delay, and
  // we only measure the total NetEQ delay, we will ask for additional delay
  // here to try to
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason significantly increased the delay.
  current_audio_delay_ms = 350;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
                MaxAudioDelayDecrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
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

TEST_F(StreamSynchronizationTest, BaseDelay) {
  int base_target_delay_ms = 2000;
  int current_audio_delay_ms = 2000;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = base_target_delay_ms;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
  // Triggering another call with the same values. Delay should not be modified.
  base_target_delay_ms = 2000;
  current_audio_delay_ms = base_target_delay_ms;
  total_video_delay_ms = base_target_delay_ms;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
  // Changing delay value - intended to test this module only. In practice it
  // would take VoE time to adapt.
  base_target_delay_ms = 5000;
  current_audio_delay_ms = base_target_delay_ms;
  total_video_delay_ms = base_target_delay_ms;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  // We are in sync don't change.
  EXPECT_FALSE(DelayedStreams(base_target_delay_ms, base_target_delay_ms,
                              current_audio_delay_ms, &extra_audio_delay_ms,
                              &total_video_delay_ms));
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLaterWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDriftWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDriftWithBaseDelay) {
  int base_target_delay_ms = 3000;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLaterWithBaseDelay) {
  int base_target_delay_ms = 2000;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterAudioClockDriftWithBaseDelay) {
  int base_target_delay_ms = 2000;
  audio_clock_drift_ = 1.05;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

TEST_F(StreamSynchronizationTest,
       BothDelayedVideoLaterVideoClockDriftWithBaseDelay) {
  int base_target_delay_ms = 2000;
  video_clock_drift_ = 1.05;
  sync_->SetTargetBufferingDelay(base_target_delay_ms);
  BothDelayedVideoLaterTest(base_target_delay_ms);
}

}  // namespace webrtc
