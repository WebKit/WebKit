/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/audio_rtp_receiver.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "media/base/media_channel.h"
#include "pc/test/mock_voice_media_receive_channel_interface.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/wait_until.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;

static const int kTimeOut = 100;
static const double kDefaultVolume = 1;
static const double kVolume = 3.7;
static const double kVolumeMuted = 0.0;
static const uint32_t kSsrc = 3;

namespace webrtc {
class AudioRtpReceiverTest : public ::testing::Test {
 protected:
  AudioRtpReceiverTest()
      : worker_thread_(Thread::Create()),
        receiver_(make_ref_counted<AudioRtpReceiver>(
            worker_thread_.get(),
            std::string(),
            std::vector<std::string>(),
            /*enable_sframe_at_owner=*/nullptr)) {
    worker_thread_->Start();
    EXPECT_CALL(receive_channel_, SetRawAudioSink(kSsrc, _));
    EXPECT_CALL(receive_channel_, SetBaseMinimumPlayoutDelayMs(kSsrc, _));
  }

  ~AudioRtpReceiverTest() override {
    EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolumeMuted));
    SetMediaChannel(nullptr);
  }

  void SetMediaChannel(MediaReceiveChannelInterface* media_channel) {
    worker_thread_->BlockingCall(
        [&]() { receiver_->SetMediaChannel(media_channel); });
  }

  test::RunLoop loop_;
  std::unique_ptr<Thread> worker_thread_;
  scoped_refptr<AudioRtpReceiver> receiver_;
  MockVoiceMediaReceiveChannelInterface receive_channel_;
};

TEST_F(AudioRtpReceiverTest, SetOutputVolumeIsCalled) {
  std::atomic_int set_volume_calls(0);

  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kDefaultVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->track();
  receiver_->track()->set_enabled(true);
  SetMediaChannel(&receive_channel_);
  EXPECT_CALL(receive_channel_, SetDefaultRawAudioSink(_)).Times(0);
  auto setup_task = receiver_->GetSetupForMediaChannel(kSsrc);
  worker_thread_->BlockingCall([&]() { std::move(setup_task)(); });

  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->OnSetVolume(kVolume);
  EXPECT_THAT(WaitUntil([&] { return set_volume_calls.load(); }, Eq(2),
                        {.timeout = TimeDelta::Millis(kTimeOut)}),
              IsRtcOk());
}

TEST_F(AudioRtpReceiverTest, VolumesSetBeforeStartingAreRespected) {
  // Set the volume before setting the media channel. It should still be used
  // as the initial volume.
  receiver_->OnSetVolume(kVolume);

  receiver_->track()->set_enabled(true);
  SetMediaChannel(&receive_channel_);

  // The previosly set initial volume should be propagated to the provided
  // media_channel_ as soon as GetSetupForMediaChannel is called.
  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolume));

  auto setup_task = receiver_->GetSetupForMediaChannel(kSsrc);
  worker_thread_->BlockingCall([&]() { std::move(setup_task)(); });
}

// Tests that OnChanged notifications are processed correctly on the worker
// thread when a media channel pointer is passed to the receiver via the
// constructor.
TEST(AudioRtpReceiver, OnChangedNotificationsAfterConstruction) {
  test::RunLoop loop;

  std::unique_ptr<Thread> worker_thread = Thread::Create();
  worker_thread->Start();
  MockVoiceMediaReceiveChannelInterface receive_channel;
  auto receiver = make_ref_counted<AudioRtpReceiver>(
      worker_thread.get(), std::string(), std::vector<std::string>(),
      /*enable_sframe_at_owner=*/nullptr, &receive_channel);

  EXPECT_CALL(receive_channel, SetDefaultRawAudioSink(_)).Times(1);
  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kDefaultVolume)).Times(1);
  auto setup_task = receiver->GetSetupForUnsignaledMediaChannel();
  worker_thread->BlockingCall([&]() { std::move(setup_task)(); });

  // When the track is marked as disabled, an async notification is queued
  // for the worker thread. This notification should trigger the volume
  // of the media channel to be set to kVolumeMuted.
  // Set the expectation first for the call, before changing the track state.
  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kVolumeMuted)).Times(1);

  // Mark the track as disabled.
  receiver->track()->set_enabled(false);

  // Flush the worker thread.
  worker_thread->BlockingCall([] {});

  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kVolumeMuted)).Times(1);
  worker_thread->BlockingCall([&]() { receiver->SetMediaChannel(nullptr); });
}

}  // namespace webrtc
