/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/utility/audio_frame_operations.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

class AudioFrameOperationsTest : public ::testing::Test {
 protected:
  AudioFrameOperationsTest() {
    // Set typical values.
    frame_.samples_per_channel_ = 320;
    frame_.num_channels_ = 2;
  }

  AudioFrame frame_;
};

void SetFrameData(int16_t ch1,
                  int16_t ch2,
                  int16_t ch3,
                  int16_t ch4,
                  AudioFrame* frame) {
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel_ * 4; i += 4) {
    frame_data[i] = ch1;
    frame_data[i + 1] = ch2;
    frame_data[i + 2] = ch3;
    frame_data[i + 3] = ch4;
  }
}

void SetFrameData(int16_t left, int16_t right, AudioFrame* frame) {
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    frame_data[i] = left;
    frame_data[i + 1] = right;
  }
}

void SetFrameData(int16_t data, AudioFrame* frame) {
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0;
       i < frame->samples_per_channel_ * frame->num_channels_; i++) {
    frame_data[i] = data;
  }
}

void VerifyFramesAreEqual(const AudioFrame& frame1, const AudioFrame& frame2) {
  EXPECT_EQ(frame1.num_channels_, frame2.num_channels_);
  EXPECT_EQ(frame1.samples_per_channel_,
            frame2.samples_per_channel_);
  const int16_t* frame1_data = frame1.data();
  const int16_t* frame2_data = frame2.data();
  for (size_t i = 0; i < frame1.samples_per_channel_ * frame1.num_channels_;
      i++) {
    EXPECT_EQ(frame1_data[i], frame2_data[i]);
  }
  EXPECT_EQ(frame1.muted(), frame2.muted());
}

void InitFrame(AudioFrame* frame, size_t channels, size_t samples_per_channel,
               int16_t left_data, int16_t right_data) {
  RTC_DCHECK(frame);
  RTC_DCHECK_GE(2, channels);
  RTC_DCHECK_GE(AudioFrame::kMaxDataSizeSamples,
                samples_per_channel * channels);
  frame->samples_per_channel_ = samples_per_channel;
  frame->num_channels_ = channels;
  if (channels == 2) {
    SetFrameData(left_data, right_data, frame);
  } else if (channels == 1) {
    SetFrameData(left_data, frame);
  }
}

int16_t GetChannelData(const AudioFrame& frame, size_t channel, size_t index) {
  RTC_DCHECK_LT(channel, frame.num_channels_);
  RTC_DCHECK_LT(index, frame.samples_per_channel_);
  return frame.data()[index * frame.num_channels_ + channel];
}

void VerifyFrameDataBounds(const AudioFrame& frame, size_t channel, int16_t max,
                           int16_t min) {
  for (size_t i = 0; i < frame.samples_per_channel_; ++i) {
    int16_t s = GetChannelData(frame, channel, i);
    EXPECT_LE(min, s);
    EXPECT_GE(max, s);
  }
}

TEST_F(AudioFrameOperationsTest, MonoToStereoFailsWithBadParameters) {
  EXPECT_EQ(-1, AudioFrameOperations::MonoToStereo(&frame_));

  frame_.samples_per_channel_ = AudioFrame::kMaxDataSizeSamples;
  frame_.num_channels_ = 1;
  EXPECT_EQ(-1, AudioFrameOperations::MonoToStereo(&frame_));
}

TEST_F(AudioFrameOperationsTest, MonoToStereoSucceeds) {
  frame_.num_channels_ = 1;
  SetFrameData(1, &frame_);

  EXPECT_EQ(0, AudioFrameOperations::MonoToStereo(&frame_));

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  stereo_frame.num_channels_ = 2;
  SetFrameData(1, 1, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, MonoToStereoMuted) {
  frame_.num_channels_ = 1;
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::MonoToStereo(&frame_));
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, MonoToStereoBufferSucceeds) {
  AudioFrame target_frame;
  frame_.num_channels_ = 1;
  SetFrameData(4, &frame_);

  target_frame.num_channels_ = 2;
  target_frame.samples_per_channel_ = frame_.samples_per_channel_;

  AudioFrameOperations::MonoToStereo(frame_.data(), frame_.samples_per_channel_,
                                     target_frame.mutable_data());

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  stereo_frame.num_channels_ = 2;
  SetFrameData(4, 4, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoFailsWithBadParameters) {
  frame_.num_channels_ = 1;
  EXPECT_EQ(-1, AudioFrameOperations::StereoToMono(&frame_));
}

TEST_F(AudioFrameOperationsTest, StereoToMonoSucceeds) {
  SetFrameData(4, 2, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::StereoToMono(&frame_));

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(3, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoMuted) {
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::StereoToMono(&frame_));
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, StereoToMonoBufferSucceeds) {
  AudioFrame target_frame;
  SetFrameData(4, 2, &frame_);

  target_frame.num_channels_ = 1;
  target_frame.samples_per_channel_ = frame_.samples_per_channel_;

  AudioFrameOperations::StereoToMono(frame_.data(), frame_.samples_per_channel_,
                                     target_frame.mutable_data());

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(3, &mono_frame);
  VerifyFramesAreEqual(mono_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoDoesNotWrapAround) {
  SetFrameData(-32768, -32768, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::StereoToMono(&frame_));

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(-32768, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoFailsWithBadParameters) {
  frame_.num_channels_ = 1;
  EXPECT_EQ(-1, AudioFrameOperations::QuadToMono(&frame_));
  frame_.num_channels_ = 2;
  EXPECT_EQ(-1, AudioFrameOperations::QuadToMono(&frame_));
}

TEST_F(AudioFrameOperationsTest, QuadToMonoSucceeds) {
  frame_.num_channels_ = 4;
  SetFrameData(4, 2, 6, 8, &frame_);

  EXPECT_EQ(0, AudioFrameOperations::QuadToMono(&frame_));

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(5, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoMuted) {
  frame_.num_channels_ = 4;
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::QuadToMono(&frame_));
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, QuadToMonoBufferSucceeds) {
  AudioFrame target_frame;
  frame_.num_channels_ = 4;
  SetFrameData(4, 2, 6, 8, &frame_);

  target_frame.num_channels_ = 1;
  target_frame.samples_per_channel_ = frame_.samples_per_channel_;

  AudioFrameOperations::QuadToMono(frame_.data(), frame_.samples_per_channel_,
                                   target_frame.mutable_data());
  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(5, &mono_frame);
  VerifyFramesAreEqual(mono_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoDoesNotWrapAround) {
  frame_.num_channels_ = 4;
  SetFrameData(-32768, -32768, -32768, -32768, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::QuadToMono(&frame_));

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  mono_frame.num_channels_ = 1;
  SetFrameData(-32768, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToStereoFailsWithBadParameters) {
  frame_.num_channels_ = 1;
  EXPECT_EQ(-1, AudioFrameOperations::QuadToStereo(&frame_));
  frame_.num_channels_ = 2;
  EXPECT_EQ(-1, AudioFrameOperations::QuadToStereo(&frame_));
}

TEST_F(AudioFrameOperationsTest, QuadToStereoSucceeds) {
  frame_.num_channels_ = 4;
  SetFrameData(4, 2, 6, 8, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::QuadToStereo(&frame_));

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  stereo_frame.num_channels_ = 2;
  SetFrameData(3, 7, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToStereoMuted) {
  frame_.num_channels_ = 4;
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::QuadToStereo(&frame_));
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, QuadToStereoBufferSucceeds) {
  AudioFrame target_frame;
  frame_.num_channels_ = 4;
  SetFrameData(4, 2, 6, 8, &frame_);

  target_frame.num_channels_ = 2;
  target_frame.samples_per_channel_ = frame_.samples_per_channel_;

  AudioFrameOperations::QuadToStereo(frame_.data(), frame_.samples_per_channel_,
                                     target_frame.mutable_data());
  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  stereo_frame.num_channels_ = 2;
  SetFrameData(3, 7, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, QuadToStereoDoesNotWrapAround) {
  frame_.num_channels_ = 4;
  SetFrameData(-32768, -32768, -32768, -32768, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::QuadToStereo(&frame_));

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  stereo_frame.num_channels_ = 2;
  SetFrameData(-32768, -32768, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsSucceedsOnStereo) {
  SetFrameData(0, 1, &frame_);

  AudioFrame swapped_frame;
  swapped_frame.samples_per_channel_ = 320;
  swapped_frame.num_channels_ = 2;
  SetFrameData(1, 0, &swapped_frame);

  AudioFrameOperations::SwapStereoChannels(&frame_);
  VerifyFramesAreEqual(swapped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsMuted) {
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::SwapStereoChannels(&frame_);
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsFailsOnMono) {
  frame_.num_channels_ = 1;
  // Set data to "stereo", despite it being a mono frame.
  SetFrameData(0, 1, &frame_);

  AudioFrame orig_frame;
  orig_frame.CopyFrom(frame_);
  AudioFrameOperations::SwapStereoChannels(&frame_);
  // Verify that no swap occurred.
  VerifyFramesAreEqual(orig_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, MuteDisabled) {
  SetFrameData(1000, -1000, &frame_);
  AudioFrameOperations::Mute(&frame_, false, false);

  AudioFrame muted_frame;
  muted_frame.samples_per_channel_ = 320;
  muted_frame.num_channels_ = 2;
  SetFrameData(1000, -1000, &muted_frame);
  VerifyFramesAreEqual(muted_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, MuteEnabled) {
  SetFrameData(1000, -1000, &frame_);
  AudioFrameOperations::Mute(&frame_, true, true);

  AudioFrame muted_frame;
  muted_frame.samples_per_channel_ = frame_.samples_per_channel_;
  muted_frame.num_channels_ = frame_.num_channels_;
  ASSERT_TRUE(muted_frame.muted());
  VerifyFramesAreEqual(muted_frame, frame_);
}

// Verify that *beginning* to mute works for short and long (>128) frames, mono
// and stereo. Beginning mute should yield a ramp down to zero.
TEST_F(AudioFrameOperationsTest, MuteBeginMonoLong) {
  InitFrame(&frame_, 1, 228, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, false, true);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 99));
  EXPECT_EQ(992, GetChannelData(frame_, 0, 100));
  EXPECT_EQ(7, GetChannelData(frame_, 0, 226));
  EXPECT_EQ(0, GetChannelData(frame_, 0, 227));
}

TEST_F(AudioFrameOperationsTest, MuteBeginMonoShort) {
  InitFrame(&frame_, 1, 93, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, false, true);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  EXPECT_EQ(989, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(978, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(10, GetChannelData(frame_, 0, 91));
  EXPECT_EQ(0, GetChannelData(frame_, 0, 92));
}

TEST_F(AudioFrameOperationsTest, MuteBeginStereoLong) {
  InitFrame(&frame_, 2, 228, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, false, true);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  VerifyFrameDataBounds(frame_, 1, 0, -1000);
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 99));
  EXPECT_EQ(-1000, GetChannelData(frame_, 1, 99));
  EXPECT_EQ(992, GetChannelData(frame_, 0, 100));
  EXPECT_EQ(-992, GetChannelData(frame_, 1, 100));
  EXPECT_EQ(7, GetChannelData(frame_, 0, 226));
  EXPECT_EQ(-7, GetChannelData(frame_, 1, 226));
  EXPECT_EQ(0, GetChannelData(frame_, 0, 227));
  EXPECT_EQ(0, GetChannelData(frame_, 1, 227));
}

TEST_F(AudioFrameOperationsTest, MuteBeginStereoShort) {
  InitFrame(&frame_, 2, 93, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, false, true);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  VerifyFrameDataBounds(frame_, 1, 0, -1000);
  EXPECT_EQ(989, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(-989, GetChannelData(frame_, 1, 0));
  EXPECT_EQ(978, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(-978, GetChannelData(frame_, 1, 1));
  EXPECT_EQ(10, GetChannelData(frame_, 0, 91));
  EXPECT_EQ(-10, GetChannelData(frame_, 1, 91));
  EXPECT_EQ(0, GetChannelData(frame_, 0, 92));
  EXPECT_EQ(0, GetChannelData(frame_, 1, 92));
}

// Verify that *ending* to mute works for short and long (>128) frames, mono
// and stereo. Ending mute should yield a ramp up from zero.
TEST_F(AudioFrameOperationsTest, MuteEndMonoLong) {
  InitFrame(&frame_, 1, 228, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, true, false);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  EXPECT_EQ(7, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(15, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 127));
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 128));
}

TEST_F(AudioFrameOperationsTest, MuteEndMonoShort) {
  InitFrame(&frame_, 1, 93, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, true, false);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  EXPECT_EQ(10, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(21, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(989, GetChannelData(frame_, 0, 91));
  EXPECT_EQ(999, GetChannelData(frame_, 0, 92));
}

TEST_F(AudioFrameOperationsTest, MuteEndStereoLong) {
  InitFrame(&frame_, 2, 228, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, true, false);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  VerifyFrameDataBounds(frame_, 1, 0, -1000);
  EXPECT_EQ(7, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(-7, GetChannelData(frame_, 1, 0));
  EXPECT_EQ(15, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(-15, GetChannelData(frame_, 1, 1));
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 127));
  EXPECT_EQ(-1000, GetChannelData(frame_, 1, 127));
  EXPECT_EQ(1000, GetChannelData(frame_, 0, 128));
  EXPECT_EQ(-1000, GetChannelData(frame_, 1, 128));
}

TEST_F(AudioFrameOperationsTest, MuteEndStereoShort) {
  InitFrame(&frame_, 2, 93, 1000, -1000);
  AudioFrameOperations::Mute(&frame_, true, false);
  VerifyFrameDataBounds(frame_, 0, 1000, 0);
  VerifyFrameDataBounds(frame_, 1, 0, -1000);
  EXPECT_EQ(10, GetChannelData(frame_, 0, 0));
  EXPECT_EQ(-10, GetChannelData(frame_, 1, 0));
  EXPECT_EQ(21, GetChannelData(frame_, 0, 1));
  EXPECT_EQ(-21, GetChannelData(frame_, 1, 1));
  EXPECT_EQ(989, GetChannelData(frame_, 0, 91));
  EXPECT_EQ(-989, GetChannelData(frame_, 1, 91));
  EXPECT_EQ(999, GetChannelData(frame_, 0, 92));
  EXPECT_EQ(-999, GetChannelData(frame_, 1, 92));
}

TEST_F(AudioFrameOperationsTest, MuteBeginAlreadyMuted) {
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::Mute(&frame_, false, true);
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, MuteEndAlreadyMuted) {
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::Mute(&frame_, true, false);
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, ApplyHalfGainSucceeds) {
  SetFrameData(2, &frame_);

  AudioFrame half_gain_frame;
  half_gain_frame.num_channels_ = frame_.num_channels_;
  half_gain_frame.samples_per_channel_ = frame_.samples_per_channel_;
  SetFrameData(1, &half_gain_frame);

  AudioFrameOperations::ApplyHalfGain(&frame_);
  VerifyFramesAreEqual(half_gain_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ApplyHalfGainMuted) {
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::ApplyHalfGain(&frame_);
  EXPECT_TRUE(frame_.muted());
}

// TODO(andrew): should not allow negative scales.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleFailsWithBadParameters) {
  frame_.num_channels_ = 1;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, 1.0, &frame_));

  frame_.num_channels_ = 3;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, 1.0, &frame_));

  frame_.num_channels_ = 2;
  EXPECT_EQ(-1, AudioFrameOperations::Scale(-1.0, 1.0, &frame_));
  EXPECT_EQ(-1, AudioFrameOperations::Scale(1.0, -1.0, &frame_));
}

// TODO(andrew): fix the wraparound bug. We should always saturate.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleDoesNotWrapAround) {
  SetFrameData(4000, -4000, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::Scale(10.0, 10.0, &frame_));

  AudioFrame clipped_frame;
  clipped_frame.samples_per_channel_ = 320;
  clipped_frame.num_channels_ = 2;
  SetFrameData(32767, -32768, &clipped_frame);
  VerifyFramesAreEqual(clipped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleSucceeds) {
  SetFrameData(1, -1, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::Scale(2.0, 3.0, &frame_));

  AudioFrame scaled_frame;
  scaled_frame.samples_per_channel_ = 320;
  scaled_frame.num_channels_ = 2;
  SetFrameData(2, -3, &scaled_frame);
  VerifyFramesAreEqual(scaled_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleMuted) {
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::Scale(2.0, 3.0, &frame_));
  EXPECT_TRUE(frame_.muted());
}

// TODO(andrew): should fail with a negative scale.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleWithSatFailsWithBadParameters) {
  EXPECT_EQ(-1, AudioFrameOperations::ScaleWithSat(-1.0, &frame_));
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatDoesNotWrapAround) {
  frame_.num_channels_ = 1;
  SetFrameData(4000, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, &frame_));

  AudioFrame clipped_frame;
  clipped_frame.samples_per_channel_ = 320;
  clipped_frame.num_channels_ = 1;
  SetFrameData(32767, &clipped_frame);
  VerifyFramesAreEqual(clipped_frame, frame_);

  SetFrameData(-4000, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, &frame_));
  SetFrameData(-32768, &clipped_frame);
  VerifyFramesAreEqual(clipped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatSucceeds) {
  frame_.num_channels_ = 1;
  SetFrameData(1, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(2.0, &frame_));

  AudioFrame scaled_frame;
  scaled_frame.samples_per_channel_ = 320;
  scaled_frame.num_channels_ = 1;
  SetFrameData(2, &scaled_frame);
  VerifyFramesAreEqual(scaled_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatMuted) {
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(2.0, &frame_));
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, AddingXToEmptyGivesX) {
  // When samples_per_channel_ is 0, the frame counts as empty and zero.
  AudioFrame frame_to_add_to;
  frame_to_add_to.mutable_data();  // Unmute the frame.
  ASSERT_FALSE(frame_to_add_to.muted());
  frame_to_add_to.samples_per_channel_ = 0;
  frame_to_add_to.num_channels_ = frame_.num_channels_;

  SetFrameData(1000, &frame_);
  AudioFrameOperations::Add(frame_, &frame_to_add_to);
  VerifyFramesAreEqual(frame_, frame_to_add_to);
}

TEST_F(AudioFrameOperationsTest, AddingXToMutedGivesX) {
  AudioFrame frame_to_add_to;
  ASSERT_TRUE(frame_to_add_to.muted());
  frame_to_add_to.samples_per_channel_ = frame_.samples_per_channel_;
  frame_to_add_to.num_channels_ = frame_.num_channels_;

  SetFrameData(1000, &frame_);
  AudioFrameOperations::Add(frame_, &frame_to_add_to);
  VerifyFramesAreEqual(frame_, frame_to_add_to);
}

TEST_F(AudioFrameOperationsTest, AddingMutedToXGivesX) {
  AudioFrame frame_to_add_to;
  frame_to_add_to.samples_per_channel_ = frame_.samples_per_channel_;
  frame_to_add_to.num_channels_ = frame_.num_channels_;
  SetFrameData(1000, &frame_to_add_to);

  AudioFrame frame_copy;
  frame_copy.CopyFrom(frame_to_add_to);

  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::Add(frame_, &frame_to_add_to);
  VerifyFramesAreEqual(frame_copy, frame_to_add_to);
}

TEST_F(AudioFrameOperationsTest, AddingTwoFramesProducesTheirSum) {
  AudioFrame frame_to_add_to;
  frame_to_add_to.samples_per_channel_ = frame_.samples_per_channel_;
  frame_to_add_to.num_channels_ = frame_.num_channels_;
  SetFrameData(1000, &frame_to_add_to);
  SetFrameData(2000, &frame_);

  AudioFrameOperations::Add(frame_, &frame_to_add_to);
  SetFrameData(frame_.data()[0] + 1000, &frame_);
  VerifyFramesAreEqual(frame_, frame_to_add_to);
}

}  // namespace
}  // namespace webrtc
