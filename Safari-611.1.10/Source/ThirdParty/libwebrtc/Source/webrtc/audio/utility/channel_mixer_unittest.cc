/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/channel_mixer.h"

#include <memory>

#include "api/audio/audio_frame.h"
#include "api/audio/channel_layout.h"
#include "audio/utility/channel_mixing_matrix.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr uint32_t kTimestamp = 27;
constexpr int kSampleRateHz = 16000;
constexpr size_t kSamplesPerChannel = kSampleRateHz / 100;

class ChannelMixerTest : public ::testing::Test {
 protected:
  ChannelMixerTest() {
    // Use 10ms audio frames by default. Don't set values yet.
    frame_.samples_per_channel_ = kSamplesPerChannel;
    frame_.sample_rate_hz_ = kSampleRateHz;
    EXPECT_TRUE(frame_.muted());
  }

  virtual ~ChannelMixerTest() {}

  AudioFrame frame_;
};

void SetFrameData(int16_t data, AudioFrame* frame) {
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel() * frame->num_channels();
       i++) {
    frame_data[i] = data;
  }
}

void SetMonoData(int16_t center, AudioFrame* frame) {
  frame->num_channels_ = 1;
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel(); ++i) {
    frame_data[i] = center;
  }
  EXPECT_FALSE(frame->muted());
}

void SetStereoData(int16_t left, int16_t right, AudioFrame* frame) {
  ASSERT_LE(2 * frame->samples_per_channel(), frame->max_16bit_samples());
  frame->num_channels_ = 2;
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel() * 2; i += 2) {
    frame_data[i] = left;
    frame_data[i + 1] = right;
  }
  EXPECT_FALSE(frame->muted());
}

void SetFiveOneData(int16_t front_left,
                    int16_t front_right,
                    int16_t center,
                    int16_t lfe,
                    int16_t side_left,
                    int16_t side_right,
                    AudioFrame* frame) {
  ASSERT_LE(6 * frame->samples_per_channel(), frame->max_16bit_samples());
  frame->num_channels_ = 6;
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel() * 6; i += 6) {
    frame_data[i] = front_left;
    frame_data[i + 1] = front_right;
    frame_data[i + 2] = center;
    frame_data[i + 3] = lfe;
    frame_data[i + 4] = side_left;
    frame_data[i + 5] = side_right;
  }
  EXPECT_FALSE(frame->muted());
}

void SetSevenOneData(int16_t front_left,
                     int16_t front_right,
                     int16_t center,
                     int16_t lfe,
                     int16_t side_left,
                     int16_t side_right,
                     int16_t back_left,
                     int16_t back_right,
                     AudioFrame* frame) {
  ASSERT_LE(8 * frame->samples_per_channel(), frame->max_16bit_samples());
  frame->num_channels_ = 8;
  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel() * 8; i += 8) {
    frame_data[i] = front_left;
    frame_data[i + 1] = front_right;
    frame_data[i + 2] = center;
    frame_data[i + 3] = lfe;
    frame_data[i + 4] = side_left;
    frame_data[i + 5] = side_right;
    frame_data[i + 6] = back_left;
    frame_data[i + 7] = back_right;
  }
  EXPECT_FALSE(frame->muted());
}

bool AllSamplesEquals(int16_t sample, const AudioFrame* frame) {
  const int16_t* frame_data = frame->data();
  for (size_t i = 0; i < frame->samples_per_channel() * frame->num_channels();
       i++) {
    if (frame_data[i] != sample) {
      return false;
    }
  }
  return true;
}

void VerifyFramesAreEqual(const AudioFrame& frame1, const AudioFrame& frame2) {
  EXPECT_EQ(frame1.num_channels(), frame2.num_channels());
  EXPECT_EQ(frame1.samples_per_channel(), frame2.samples_per_channel());
  const int16_t* frame1_data = frame1.data();
  const int16_t* frame2_data = frame2.data();
  for (size_t i = 0; i < frame1.samples_per_channel() * frame1.num_channels();
       i++) {
    EXPECT_EQ(frame1_data[i], frame2_data[i]);
  }
  EXPECT_EQ(frame1.muted(), frame2.muted());
}

}  // namespace

// Test all possible layout conversions can be constructed and mixed. Don't
// care about the actual content, simply run through all mixing combinations
// and ensure that nothing fails.
TEST_F(ChannelMixerTest, ConstructAllPossibleLayouts) {
  for (ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
       input_layout <= CHANNEL_LAYOUT_MAX;
       input_layout = static_cast<ChannelLayout>(input_layout + 1)) {
    for (ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
         output_layout <= CHANNEL_LAYOUT_MAX;
         output_layout = static_cast<ChannelLayout>(output_layout + 1)) {
      // DISCRETE, BITSTREAM can't be tested here based on the current approach.
      // CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC is not mixable.
      // Stereo down mix should never be the output layout.
      if (input_layout == CHANNEL_LAYOUT_BITSTREAM ||
          input_layout == CHANNEL_LAYOUT_DISCRETE ||
          input_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_BITSTREAM ||
          output_layout == CHANNEL_LAYOUT_DISCRETE ||
          output_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_STEREO_DOWNMIX) {
        continue;
      }

      rtc::StringBuilder ss;
      ss << "Input Layout: " << input_layout
         << ", Output Layout: " << output_layout;
      SCOPED_TRACE(ss.str());
      ChannelMixer mixer(input_layout, output_layout);

      frame_.UpdateFrame(kTimestamp, nullptr, kSamplesPerChannel, kSampleRateHz,
                         AudioFrame::kNormalSpeech, AudioFrame::kVadActive,
                         ChannelLayoutToChannelCount(input_layout));
      EXPECT_TRUE(frame_.muted());
      mixer.Transform(&frame_);
    }
  }
}

// Ensure that the audio frame is untouched when input and output channel
// layouts are identical, i.e., the transformation should have no effect.
// Exclude invalid mixing combinations.
TEST_F(ChannelMixerTest, NoMixingForIdenticalChannelLayouts) {
  for (ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
       input_layout <= CHANNEL_LAYOUT_MAX;
       input_layout = static_cast<ChannelLayout>(input_layout + 1)) {
    for (ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
         output_layout <= CHANNEL_LAYOUT_MAX;
         output_layout = static_cast<ChannelLayout>(output_layout + 1)) {
      if (input_layout != output_layout ||
          input_layout == CHANNEL_LAYOUT_BITSTREAM ||
          input_layout == CHANNEL_LAYOUT_DISCRETE ||
          input_layout == CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC ||
          output_layout == CHANNEL_LAYOUT_STEREO_DOWNMIX) {
        continue;
      }
      ChannelMixer mixer(input_layout, output_layout);
      frame_.num_channels_ = ChannelLayoutToChannelCount(input_layout);
      SetFrameData(99, &frame_);
      mixer.Transform(&frame_);
      EXPECT_EQ(ChannelLayoutToChannelCount(input_layout),
                static_cast<int>(frame_.num_channels()));
      EXPECT_TRUE(AllSamplesEquals(99, &frame_));
    }
  }
}

TEST_F(ChannelMixerTest, StereoToMono) {
  ChannelMixer mixer(CHANNEL_LAYOUT_STEREO, CHANNEL_LAYOUT_MONO);
  //
  //                      Input: stereo
  //                      LEFT  RIGHT
  // Output: mono CENTER  0.5   0.5
  //
  SetStereoData(7, 3, &frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame_.channel_layout());

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetMonoData(5, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);

  SetStereoData(-32768, -32768, &frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame_.channel_layout());
  SetMonoData(-32768, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(ChannelMixerTest, StereoToMonoMuted) {
  ASSERT_TRUE(frame_.muted());
  ChannelMixer mixer(CHANNEL_LAYOUT_STEREO, CHANNEL_LAYOUT_MONO);
  mixer.Transform(&frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame_.channel_layout());
  EXPECT_TRUE(frame_.muted());
}

TEST_F(ChannelMixerTest, FiveOneToSevenOneMuted) {
  ASSERT_TRUE(frame_.muted());
  ChannelMixer mixer(CHANNEL_LAYOUT_5_1, CHANNEL_LAYOUT_7_1);
  mixer.Transform(&frame_);
  EXPECT_EQ(8u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_7_1, frame_.channel_layout());
  EXPECT_TRUE(frame_.muted());
}

TEST_F(ChannelMixerTest, FiveOneToMono) {
  ChannelMixer mixer(CHANNEL_LAYOUT_5_1, CHANNEL_LAYOUT_MONO);
  //
  //                      Input: 5.1
  //                      LEFT   RIGHT  CENTER  LFE    SIDE_LEFT  SIDE_RIGHT
  // Output: mono CENTER  0.707  0.707  1       0.707  0.707      0.707
  //
  // a = [10, 20, 15, 2, 5, 5]
  // b = [1/sqrt(2), 1/sqrt(2), 1.0, 1/sqrt(2), 1/sqrt(2), 1/sqrt(2)] =>
  // a * b (dot product) = 44.69848480983499,
  // which is truncated into 44 using 16 bit representation.
  //
  SetFiveOneData(10, 20, 15, 2, 5, 5, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame_.channel_layout());

  AudioFrame mono_frame;
  mono_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetMonoData(44, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);

  SetFiveOneData(-32768, -32768, -32768, -32768, -32768, -32768, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_MONO, frame_.channel_layout());
  SetMonoData(-32768, &mono_frame);
  VerifyFramesAreEqual(mono_frame, frame_);
}

TEST_F(ChannelMixerTest, FiveOneToSevenOne) {
  ChannelMixer mixer(CHANNEL_LAYOUT_5_1, CHANNEL_LAYOUT_7_1);
  //
  //                        Input: 5.1
  //                        LEFT   RIGHT  CENTER  LFE    SIDE_LEFT  SIDE_RIGHT
  // Output: 7.1 LEFT       1      0      0       0      0          0
  //             RIGHT      0      1      0       0      0          0
  //             CENTER     0      0      1       0      0          0
  //             LFE        0      0      0       1      0          0
  //             SIDE_LEFT  0      0      0       0      1          0
  //             SIDE_RIGHT 0      0      0       0      0          1
  //             BACK_LEFT  0      0      0       0      0          0
  //             BACK_RIGHT 0      0      0       0      0          0
  //
  SetFiveOneData(10, 20, 15, 2, 5, 5, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(8u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_7_1, frame_.channel_layout());

  AudioFrame seven_one_frame;
  seven_one_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetSevenOneData(10, 20, 15, 2, 5, 5, 0, 0, &seven_one_frame);
  VerifyFramesAreEqual(seven_one_frame, frame_);

  SetFiveOneData(-32768, 32767, -32768, 32767, -32768, 32767, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(8u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_7_1, frame_.channel_layout());
  SetSevenOneData(-32768, 32767, -32768, 32767, -32768, 32767, 0, 0,
                  &seven_one_frame);
  VerifyFramesAreEqual(seven_one_frame, frame_);
}

TEST_F(ChannelMixerTest, FiveOneBackToStereo) {
  ChannelMixer mixer(CHANNEL_LAYOUT_5_1_BACK, CHANNEL_LAYOUT_STEREO);
  //
  //                      Input: 5.1
  //                      LEFT   RIGHT  CENTER  LFE    BACK_LEFT  BACK_RIGHT
  // Output: stereo LEFT  1      0      0.707   0.707  0.707      0
  //                RIGHT 0      1      0.707   0.707  0          0.707
  //
  SetFiveOneData(20, 30, 15, 2, 5, 5, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, frame_.channel_layout());

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetStereoData(35, 45, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);

  SetFiveOneData(-32768, -32768, -32768, -32768, -32768, -32768, &frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, frame_.channel_layout());
  SetStereoData(-32768, -32768, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(ChannelMixerTest, MonoToStereo) {
  ChannelMixer mixer(CHANNEL_LAYOUT_MONO, CHANNEL_LAYOUT_STEREO);
  //
  //                       Input: mono
  //                       CENTER
  // Output: stereo LEFT   1
  //                RIGHT  1
  //
  SetMonoData(44, &frame_);
  EXPECT_EQ(1u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, frame_.channel_layout());

  AudioFrame stereo_frame;
  stereo_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetStereoData(44, 44, &stereo_frame);
  VerifyFramesAreEqual(stereo_frame, frame_);
}

TEST_F(ChannelMixerTest, StereoToFiveOne) {
  ChannelMixer mixer(CHANNEL_LAYOUT_STEREO, CHANNEL_LAYOUT_5_1);
  //
  //                         Input: Stereo
  //                         LEFT   RIGHT
  // Output: 5.1 LEFT        1      0
  //             RIGHT       0      1
  //             CENTER      0      0
  //             LFE         0      0
  //             SIDE_LEFT   0      0
  //             SIDE_RIGHT  0      0
  //
  SetStereoData(50, 60, &frame_);
  EXPECT_EQ(2u, frame_.num_channels());
  mixer.Transform(&frame_);
  EXPECT_EQ(6u, frame_.num_channels());
  EXPECT_EQ(CHANNEL_LAYOUT_5_1, frame_.channel_layout());

  AudioFrame five_one_frame;
  five_one_frame.samples_per_channel_ = frame_.samples_per_channel();
  SetFiveOneData(50, 60, 0, 0, 0, 0, &five_one_frame);
  VerifyFramesAreEqual(five_one_frame, frame_);
}

}  // namespace webrtc
