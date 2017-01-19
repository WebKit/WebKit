/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/base/thread.h"
#include "webrtc/pc/audiomonitor.h"
#include "webrtc/pc/currentspeakermonitor.h"

namespace cricket {

static const uint32_t kSsrc1 = 1001;
static const uint32_t kSsrc2 = 1002;
static const uint32_t kMinTimeBetweenSwitches = 10;
// Due to limited system clock resolution, the CurrentSpeakerMonitor may
// actually require more or less time between switches than that specified
// in the call to set_min_time_between_switches.  To be safe, we sleep for
// 90 ms more than the min time between switches before checking for a switch.
// I am assuming system clocks do not have a coarser resolution than 90 ms.
static const uint32_t kSleepTimeBetweenSwitches = 100;

class CurrentSpeakerMonitorTest : public testing::Test,
    public sigslot::has_slots<> {
 public:
  CurrentSpeakerMonitorTest() {
    monitor_ = new CurrentSpeakerMonitor(&source_);
    // Shrink the minimum time betweeen switches to 10 ms so we don't have to
    // slow down our tests.
    monitor_->set_min_time_between_switches(kMinTimeBetweenSwitches);
    monitor_->SignalUpdate.connect(this, &CurrentSpeakerMonitorTest::OnUpdate);
    current_speaker_ = 0;
    num_changes_ = 0;
    monitor_->Start();
  }

  ~CurrentSpeakerMonitorTest() {
    delete monitor_;
  }

  void SignalAudioMonitor(const AudioInfo& info) {
    source_.SignalAudioMonitor(&source_, info);
  }

 protected:
  AudioSourceContext source_;
  CurrentSpeakerMonitor* monitor_;
  int num_changes_;
  uint32_t current_speaker_;

  void OnUpdate(CurrentSpeakerMonitor* monitor, uint32_t current_speaker) {
    current_speaker_ = current_speaker;
    num_changes_++;
  }
};

static void InitAudioInfo(AudioInfo* info, int input_level, int output_level) {
  info->input_level = input_level;
  info->output_level = output_level;
}

TEST_F(CurrentSpeakerMonitorTest, NoActiveStreams) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);
}

TEST_F(CurrentSpeakerMonitorTest, MultipleActiveStreams) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  // No speaker recognized because the initial sample is treated as possibly
  // just noise and disregarded.
  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);
}

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST_F(CurrentSpeakerMonitorTest, DISABLED_RapidSpeakerChange) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 9));
  info.active_streams.push_back(std::make_pair(kSsrc2, 1));
  SignalAudioMonitor(info);

  // We expect no speaker change because of the rapid change.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);
}

TEST_F(CurrentSpeakerMonitorTest, SpeakerChange) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  // Wait so the changes don't come so rapidly.
  rtc::Thread::SleepMs(kSleepTimeBetweenSwitches);

  info.active_streams.push_back(std::make_pair(kSsrc1, 9));
  info.active_streams.push_back(std::make_pair(kSsrc2, 1));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc1);
  EXPECT_EQ(num_changes_, 2);
}

TEST_F(CurrentSpeakerMonitorTest, InterwordSilence) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  SignalAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  // Wait so the changes don't come so rapidly.
  rtc::Thread::SleepMs(kSleepTimeBetweenSwitches);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  SignalAudioMonitor(info);

  // Current speaker shouldn't have changed because we treat this as an inter-
  // word silence.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  SignalAudioMonitor(info);

  // Current speaker shouldn't have changed because we treat this as an inter-
  // word silence.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  SignalAudioMonitor(info);

  // At this point, we should have concluded that SSRC2 stopped speaking.
  EXPECT_EQ(current_speaker_, kSsrc1);
  EXPECT_EQ(num_changes_, 2);
}

}  // namespace cricket
