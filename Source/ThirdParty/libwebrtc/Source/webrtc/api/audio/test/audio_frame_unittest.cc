/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/audio_frame.h"

#include <stdint.h>
#include <string.h>  // memcmp

#include "test/gtest.h"

namespace webrtc {

namespace {

bool AllSamplesAre(int16_t sample, rtc::ArrayView<const int16_t> samples) {
  for (const auto s : samples) {
    if (s != sample) {
      return false;
    }
  }
  return true;
}

bool AllSamplesAre(int16_t sample, const AudioFrame& frame) {
  return AllSamplesAre(sample, frame.data_view());
}

// Checks the values of samples in the AudioFrame buffer, regardless of whether
// they're valid or not, and disregard the `muted()` state of the frame.
// I.e. use `max_16bit_samples()` instead of the audio properties
// `num_samples * samples_per_channel`.
bool AllBufferSamplesAre(int16_t sample, const AudioFrame& frame) {
  const auto* data = frame.data_view().data();
  for (size_t i = 0; i < frame.max_16bit_samples(); ++i) {
    if (data[i] != sample) {
      return false;
    }
  }
  return true;
}

constexpr uint32_t kTimestamp = 27;
constexpr int kSampleRateHz = 16000;
constexpr size_t kNumChannelsMono = 1;
constexpr size_t kNumChannelsStereo = 2;
constexpr size_t kNumChannels5_1 = 6;
constexpr size_t kSamplesPerChannel = kSampleRateHz / 100;

}  // namespace

TEST(AudioFrameTest, FrameStartsZeroedAndMuted) {
  AudioFrame frame;
  EXPECT_TRUE(frame.muted());
  EXPECT_TRUE(frame.data_view().empty());
  EXPECT_TRUE(AllSamplesAre(0, frame));
}

// TODO: b/335805780 - Delete test when `mutable_data()` returns ArrayView.
TEST(AudioFrameTest, UnmutedFrameIsInitiallyZeroedLegacy) {
  AudioFrame frame(kSampleRateHz, kNumChannelsMono, CHANNEL_LAYOUT_NONE);
  frame.mutable_data();
  EXPECT_FALSE(frame.muted());
  EXPECT_TRUE(AllSamplesAre(0, frame));
  EXPECT_TRUE(AllBufferSamplesAre(0, frame));
}

TEST(AudioFrameTest, UnmutedFrameIsInitiallyZeroed) {
  AudioFrame frame;
  auto data = frame.mutable_data(kSamplesPerChannel, kNumChannelsMono);
  EXPECT_FALSE(frame.muted());
  EXPECT_EQ(frame.data_view().size(), kSamplesPerChannel);
  EXPECT_EQ(data.size(), kSamplesPerChannel);
  EXPECT_TRUE(AllSamplesAre(0, frame));
}

TEST(AudioFrameTest, MutedFrameBufferIsZeroed) {
  AudioFrame frame;
  int16_t* frame_data =
      frame.mutable_data(kSamplesPerChannel, kNumChannelsMono).begin();
  EXPECT_FALSE(frame.muted());
  // Fill the reserved buffer with non-zero data.
  for (size_t i = 0; i < frame.max_16bit_samples(); i++) {
    frame_data[i] = 17;
  }
  ASSERT_TRUE(AllSamplesAre(17, frame));
  ASSERT_TRUE(AllBufferSamplesAre(17, frame));
  frame.Mute();
  EXPECT_TRUE(frame.muted());
  EXPECT_TRUE(AllSamplesAre(0, frame));
  ASSERT_TRUE(AllBufferSamplesAre(0, frame));
}

TEST(AudioFrameTest, UpdateFrameMono) {
  AudioFrame frame;
  int16_t samples[kNumChannelsMono * kSamplesPerChannel] = {17};
  frame.UpdateFrame(kTimestamp, samples, kSamplesPerChannel, kSampleRateHz,
                    AudioFrame::kPLC, AudioFrame::kVadActive, kNumChannelsMono);

  EXPECT_EQ(kTimestamp, frame.timestamp_);
  EXPECT_EQ(kSamplesPerChannel, frame.samples_per_channel());
  EXPECT_EQ(kSampleRateHz, frame.sample_rate_hz());
  EXPECT_EQ(AudioFrame::kPLC, frame.speech_type_);
  EXPECT_EQ(AudioFrame::kVadActive, frame.vad_activity_);
  EXPECT_EQ(kNumChannelsMono, frame.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame.channel_layout());

  EXPECT_FALSE(frame.muted());
  EXPECT_EQ(0, memcmp(samples, frame.data(), sizeof(samples)));

  frame.UpdateFrame(kTimestamp, nullptr /* data*/, kSamplesPerChannel,
                    kSampleRateHz, AudioFrame::kPLC, AudioFrame::kVadActive,
                    kNumChannelsMono);
  EXPECT_TRUE(frame.muted());
  EXPECT_TRUE(AllSamplesAre(0, frame));
}

TEST(AudioFrameTest, UpdateFrameMultiChannel) {
  AudioFrame frame;
  frame.UpdateFrame(kTimestamp, nullptr /* data */, kSamplesPerChannel,
                    kSampleRateHz, AudioFrame::kPLC, AudioFrame::kVadActive,
                    kNumChannelsStereo);
  EXPECT_EQ(kSamplesPerChannel, frame.samples_per_channel());
  EXPECT_EQ(kNumChannelsStereo, frame.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, frame.channel_layout());
  EXPECT_TRUE(frame.muted());

  // Initialize the frame with valid `kNumChannels5_1` data to make sure we
  // get an unmuted frame with valid samples.
  int16_t samples[kSamplesPerChannel * kNumChannels5_1] = {17};
  frame.UpdateFrame(kTimestamp, samples /* data */, kSamplesPerChannel,
                    kSampleRateHz, AudioFrame::kPLC, AudioFrame::kVadActive,
                    kNumChannels5_1);
  EXPECT_FALSE(frame.muted());
  EXPECT_EQ(kSamplesPerChannel, frame.samples_per_channel());
  EXPECT_EQ(kSamplesPerChannel * kNumChannels5_1, frame.data_view().size());
  EXPECT_EQ(kNumChannels5_1, frame.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_5_1, frame.channel_layout());
}

TEST(AudioFrameTest, CopyFrom) {
  AudioFrame frame1;
  AudioFrame frame2;

  int16_t samples[kNumChannelsMono * kSamplesPerChannel] = {17};
  frame2.UpdateFrame(kTimestamp, samples, kSamplesPerChannel, kSampleRateHz,
                     AudioFrame::kPLC, AudioFrame::kVadActive,
                     kNumChannelsMono);
  frame1.CopyFrom(frame2);

  EXPECT_EQ(frame2.timestamp_, frame1.timestamp_);
  EXPECT_EQ(frame2.samples_per_channel_, frame1.samples_per_channel_);
  EXPECT_EQ(frame2.sample_rate_hz_, frame1.sample_rate_hz_);
  EXPECT_EQ(frame2.speech_type_, frame1.speech_type_);
  EXPECT_EQ(frame2.vad_activity_, frame1.vad_activity_);
  EXPECT_EQ(frame2.num_channels_, frame1.num_channels_);

  EXPECT_EQ(frame2.data_view().size(), frame1.data_view().size());
  EXPECT_EQ(frame2.muted(), frame1.muted());
  EXPECT_EQ(0, memcmp(frame2.data(), frame1.data(), sizeof(samples)));

  frame2.UpdateFrame(kTimestamp, nullptr /* data */, kSamplesPerChannel,
                     kSampleRateHz, AudioFrame::kPLC, AudioFrame::kVadActive,
                     kNumChannelsMono);
  frame1.CopyFrom(frame2);

  EXPECT_EQ(frame2.muted(), frame1.muted());
  EXPECT_EQ(0, memcmp(frame2.data(), frame1.data(), sizeof(samples)));
}

}  // namespace webrtc
