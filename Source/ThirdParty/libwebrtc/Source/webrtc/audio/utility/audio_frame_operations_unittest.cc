/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/audio_frame_operations.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class AudioFrameOperationsTest : public ::testing::Test {
 protected:
  AudioFrameOperationsTest() = default;

  // Set typical values.
  AudioFrame frame_{/*sample_rate=*/32000, /*num_channels*/ 2};
};

class AudioFrameOperationsDeathTest : public AudioFrameOperationsTest {};

void SetFrameData(int16_t ch1,
                  int16_t ch2,
                  int16_t ch3,
                  int16_t ch4,
                  AudioFrame* frame) {
  rtc::ArrayView<int16_t> frame_data =
      frame->mutable_data(frame->samples_per_channel_, 4);
  for (size_t i = 0; i < frame->samples_per_channel_ * 4; i += 4) {
    frame_data[i] = ch1;
    frame_data[i + 1] = ch2;
    frame_data[i + 2] = ch3;
    frame_data[i + 3] = ch4;
  }
}

void SetFrameData(int16_t left, int16_t right, AudioFrame* frame) {
  rtc::ArrayView<int16_t> frame_data =
      frame->mutable_data(frame->samples_per_channel_, 2);
  for (size_t i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    frame_data[i] = left;
    frame_data[i + 1] = right;
  }
}

void SetFrameData(int16_t data, AudioFrame* frame) {
  rtc::ArrayView<int16_t> frame_data =
      frame->mutable_data(frame->samples_per_channel_, 1);
  for (size_t i = 0; i < frame->samples_per_channel_ * frame->num_channels_;
       i++) {
    frame_data[i] = data;
  }
}

void VerifyFramesAreEqual(const AudioFrame& frame1, const AudioFrame& frame2) {
  ASSERT_EQ(frame1.num_channels_, frame2.num_channels_);
  ASSERT_EQ(frame1.samples_per_channel_, frame2.samples_per_channel_);
  EXPECT_EQ(frame1.muted(), frame2.muted());
  const int16_t* frame1_data = frame1.data();
  const int16_t* frame2_data = frame2.data();
  // TODO(tommi): Use sample_count() or data_view().
  for (size_t i = 0; i < frame1.samples_per_channel_ * frame1.num_channels_;
       i++) {
    EXPECT_EQ(frame1_data[i], frame2_data[i]);
    if (frame1_data[i] != frame2_data[i])
      break;  // To avoid spamming the log.
  }
}

void InitFrame(AudioFrame* frame,
               size_t channels,
               size_t samples_per_channel,
               int16_t left_data,
               int16_t right_data) {
  RTC_DCHECK_GE(2, channels);
  RTC_DCHECK_GE(AudioFrame::kMaxDataSizeSamples,
                samples_per_channel * channels);
  frame->samples_per_channel_ = samples_per_channel;
  if (channels == 2) {
    SetFrameData(left_data, right_data, frame);
  } else if (channels == 1) {
    SetFrameData(left_data, frame);
  }
  ASSERT_EQ(frame->num_channels_, channels);
}

int16_t GetChannelData(const AudioFrame& frame, size_t channel, size_t index) {
  RTC_DCHECK_LT(channel, frame.num_channels_);
  RTC_DCHECK_LT(index, frame.samples_per_channel_);
  return frame.data()[index * frame.num_channels_ + channel];
}

void VerifyFrameDataBounds(const AudioFrame& frame,
                           size_t channel,
                           int16_t max,
                           int16_t min) {
  for (size_t i = 0; i < frame.samples_per_channel_; ++i) {
    int16_t s = GetChannelData(frame, channel, i);
    EXPECT_LE(min, s);
    EXPECT_GE(max, s);
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(AudioFrameOperationsDeathTest, MonoToStereoFailsWithBadParameters) {
  EXPECT_DEATH(AudioFrameOperations::UpmixChannels(2, &frame_), "");
  frame_.samples_per_channel_ = AudioFrame::kMaxDataSizeSamples;
  frame_.num_channels_ = 1;
  EXPECT_DEATH(AudioFrameOperations::UpmixChannels(2, &frame_), "");
}
#endif

TEST_F(AudioFrameOperationsTest, MonoToStereoSucceeds) {
  SetFrameData(1, &frame_);

  AudioFrameOperations::UpmixChannels(2, &frame_);
  EXPECT_EQ(2u, frame_.num_channels_);

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  SetFrameData(1, 1, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, MonoToStereoMuted) {
  frame_.num_channels_ = 1;
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::UpmixChannels(2, &frame_);
  EXPECT_EQ(2u, frame_.num_channels_);
  EXPECT_TRUE(frame_.muted());
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(AudioFrameOperationsDeathTest, StereoToMonoFailsWithBadParameters) {
  frame_.num_channels_ = 1;
  EXPECT_DEATH(AudioFrameOperations::DownmixChannels(1, &frame_), "");
}
#endif

TEST_F(AudioFrameOperationsTest, StereoToMonoSucceeds) {
  SetFrameData(4, 2, &frame_);
  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  SetFrameData(3, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoMuted) {
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, StereoToMonoBufferSucceeds) {
  AudioFrame target_frame;
  SetFrameData(4, 2, &frame_);

  AudioFrameOperations::DownmixChannels(
      frame_.data_view(), 2, frame_.samples_per_channel_, 1,
      target_frame.mutable_data(frame_.samples_per_channel_, 1));

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  SetFrameData(3, &mono_frame);
  VerifyFramesAreEqual(mono_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, StereoToMonoDoesNotWrapAround) {
  SetFrameData(-32768, -32768, &frame_);
  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);
  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  SetFrameData(-32768, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoSucceeds) {
  SetFrameData(4, 2, 6, 8, &frame_);

  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  SetFrameData(5, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoMuted) {
  frame_.num_channels_ = 4;
  ASSERT_TRUE(frame_.muted());
  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);
  EXPECT_TRUE(frame_.muted());
}

TEST_F(AudioFrameOperationsTest, QuadToMonoBufferSucceeds) {
  AudioFrame target_frame;
  SetFrameData(4, 2, 6, 8, &frame_);

  AudioFrameOperations::DownmixChannels(
      frame_.data_view(), 4, frame_.samples_per_channel_, 1,
      target_frame.mutable_data(frame_.samples_per_channel_, 1));
  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
  SetFrameData(5, &mono_frame);
  VerifyFramesAreEqual(mono_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, QuadToMonoDoesNotWrapAround) {
  SetFrameData(-32768, -32768, -32768, -32768, &frame_);
  AudioFrameOperations::DownmixChannels(1, &frame_);
  EXPECT_EQ(1u, frame_.num_channels_);

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = 320;
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
  SetFrameData(4, 2, 6, 8, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::QuadToStereo(&frame_));

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
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
  SetFrameData(4, 2, 6, 8, &frame_);

  AudioFrameOperations::QuadToStereo(
      frame_.data_view(), frame_.samples_per_channel_,
      target_frame.mutable_data(frame_.samples_per_channel_, 2));
  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  SetFrameData(3, 7, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, target_frame);
}

TEST_F(AudioFrameOperationsTest, QuadToStereoDoesNotWrapAround) {
  SetFrameData(-32768, -32768, -32768, -32768, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::QuadToStereo(&frame_));

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = 320;
  SetFrameData(-32768, -32768, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, SwapStereoChannelsSucceedsOnStereo) {
  SetFrameData(0, 1, &frame_);

  AudioFrame swapped_frame;
  swapped_frame.samples_per_channel_ = 320;
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
  // Set data to "stereo", despite it being a mono frame.
  SetFrameData(0, 1, &frame_);
  frame_.num_channels_ = 1;  // Reset to mono after SetFrameData().

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

// TODO(andrew): should fail with a negative scale.
TEST_F(AudioFrameOperationsTest, DISABLED_ScaleWithSatFailsWithBadParameters) {
  EXPECT_EQ(-1, AudioFrameOperations::ScaleWithSat(-1.0, &frame_));
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatDoesNotWrapAround) {
  SetFrameData(4000, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, &frame_));

  AudioFrame clipped_frame;
  clipped_frame.samples_per_channel_ = 320;
  SetFrameData(32767, &clipped_frame);
  VerifyFramesAreEqual(clipped_frame, frame_);

  SetFrameData(-4000, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(10.0, &frame_));
  SetFrameData(-32768, &clipped_frame);
  VerifyFramesAreEqual(clipped_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatSucceeds) {
  SetFrameData(1, &frame_);
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(2.0, &frame_));

  AudioFrame scaled_frame;
  scaled_frame.samples_per_channel_ = 320;
  SetFrameData(2, &scaled_frame);
  VerifyFramesAreEqual(scaled_frame, frame_);
}

TEST_F(AudioFrameOperationsTest, ScaleWithSatMuted) {
  ASSERT_TRUE(frame_.muted());
  EXPECT_EQ(0, AudioFrameOperations::ScaleWithSat(2.0, &frame_));
  EXPECT_TRUE(frame_.muted());
}

}  // namespace
}  // namespace webrtc
