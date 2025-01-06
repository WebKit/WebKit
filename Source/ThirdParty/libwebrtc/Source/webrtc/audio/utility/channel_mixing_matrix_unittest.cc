/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/channel_mixing_matrix.h"

#include <stddef.h>

#include "audio/utility/channel_mixer.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

// Test all possible layout conversions can be constructed and mixed.
// Also ensure that the channel matrix fulfill certain conditions when remapping
// is supported.
TEST(ChannelMixingMatrixTest, ConstructAllPossibleLayouts) {
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
      ChannelMixingMatrix matrix_builder(
          input_layout, ChannelLayoutToChannelCount(input_layout),
          output_layout, ChannelLayoutToChannelCount(output_layout));
      const int input_channels = ChannelLayoutToChannelCount(input_layout);
      const int output_channels = ChannelLayoutToChannelCount(output_layout);
      std::vector<std::vector<float>> matrix;
      bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

      if (remapping) {
        // Also ensure that (when remapping can take place), a maximum of one
        // input channel is included per output. This knowledge will simplify
        // the channel mixing algorithm since it allows us to find the only
        // scale factor which equals 1.0 and copy that input to its
        // corresponding output. If no such factor can be found, the
        // corresponding output can be set to zero.
        for (int i = 0; i < output_channels; i++) {
          EXPECT_EQ(static_cast<size_t>(input_channels), matrix[i].size());
          int num_input_channels_accounted_for_per_output = 0;
          for (int j = 0; j < input_channels; j++) {
            float scale = matrix[i][j];
            if (scale > 0) {
              EXPECT_EQ(scale, 1.0f);
              num_input_channels_accounted_for_per_output++;
            }
          }
          // Each output channel shall contain contribution from one or less
          // input channels.
          EXPECT_LE(num_input_channels_accounted_for_per_output, 1);
        }
      }
    }
  }
}

// Verify channels are mixed and scaled correctly.
TEST(ChannelMixingMatrixTest, StereoToMono) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_STEREO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                      Input: stereo
  //                      LEFT  RIGHT
  // Output: mono CENTER  0.5   0.5
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(2u, matrix[0].size());
  EXPECT_EQ(0.5f, matrix[0][0]);
  EXPECT_EQ(0.5f, matrix[0][1]);
}

TEST(ChannelMixingMatrixTest, MonoToStereo) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_STEREO;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                       Input: mono
  //                       CENTER
  // Output: stereo LEFT   1
  //                RIGHT  1
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(1u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(1u, matrix[1].size());
  EXPECT_EQ(1.0f, matrix[1][0]);
}

TEST(ChannelMixingMatrixTest, MonoToTwoOne) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_2_1;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                         Input: mono
  //                         CENTER
  // Output: 2.1 FRONT_LEFT  1
  //             FRONT_RIGHT 1
  //             BACK_CENTER 0
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(3u, matrix.size());
  EXPECT_EQ(1u, matrix[0].size());
  EXPECT_EQ(1.0f, matrix[0][0]);
  EXPECT_EQ(1.0f, matrix[1][0]);
  EXPECT_EQ(0.0f, matrix[2][0]);
}

TEST(ChannelMixingMatrixTest, MonoToFiveOne) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_5_1;
  const int input_channels = ChannelLayoutToChannelCount(input_layout);
  const int output_channels = ChannelLayoutToChannelCount(output_layout);
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);
  //                         Input: mono
  //                         CENTER
  // Output: 5.1 LEFT        1
  //             RIGHT       1
  //             CENTER      0
  //             LFE         0
  //             SIDE_LEFT   0
  //             SIDE_RIGHT  0
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
  for (int n = 0; n < output_channels; n++) {
    EXPECT_EQ(static_cast<size_t>(input_channels), matrix[n].size());
    if (n == LEFT || n == RIGHT) {
      EXPECT_EQ(1.0f, matrix[n][0]);
    } else {
      EXPECT_EQ(0.0f, matrix[n][0]);
    }
  }
}

TEST(ChannelMixingMatrixTest, MonoToSevenOne) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_MONO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_7_1;
  const int input_channels = ChannelLayoutToChannelCount(input_layout);
  const int output_channels = ChannelLayoutToChannelCount(output_layout);
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);
  //                         Input: mono
  //                         CENTER
  // Output: 7.1 LEFT        1
  //             RIGHT       1
  //             CENTER      0
  //             LFE         0
  //             SIDE_LEFT   0
  //             SIDE_RIGHT  0
  //             BACK_LEFT   0
  //             BACK_RIGHT  0
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
  for (int n = 0; n < output_channels; n++) {
    EXPECT_EQ(static_cast<size_t>(input_channels), matrix[n].size());
    if (n == LEFT || n == RIGHT) {
      EXPECT_EQ(1.0f, matrix[n][0]);
    } else {
      EXPECT_EQ(0.0f, matrix[n][0]);
    }
  }
}

TEST(ChannelMixingMatrixTest, FiveOneToMono) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_5_1;
  ChannelLayout output_layout = CHANNEL_LAYOUT_MONO;
  ChannelMixingMatrix matrix_builder(
      input_layout, ChannelLayoutToChannelCount(input_layout), output_layout,
      ChannelLayoutToChannelCount(output_layout));
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  //
  //                      Input: 5.1
  //                      LEFT   RIGHT  CENTER  LFE    SIDE_LEFT  SIDE_RIGHT
  // Output: mono CENTER  0.707  0.707  1       0.707  0.707      0.707
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(6u, matrix[0].size());
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][0]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][1]);
  // The center channel will be mixed at scale 1.
  EXPECT_EQ(1.0f, matrix[0][2]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][3]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][4]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[0][5]);
}

TEST(ChannelMixingMatrixTest, FiveOneBackToStereo) {
  // Front L, Front R, Front C, LFE, Back L, Back R
  ChannelLayout input_layout = CHANNEL_LAYOUT_5_1_BACK;
  ChannelLayout output_layout = CHANNEL_LAYOUT_STEREO;
  const int input_channels = ChannelLayoutToChannelCount(input_layout);
  const int output_channels = ChannelLayoutToChannelCount(output_layout);
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  // Note: 1/sqrt(2) is shown as 0.707.
  // Note: The Channels enumerator is given by {LEFT = 0, RIGHT, CENTER, LFE,
  // BACK_LEFT, BACK_RIGHT,...}, hence we can use the enumerator values as
  // indexes in the matrix when verifying the scaling factors.
  //
  //                      Input: 5.1
  //                      LEFT   RIGHT  CENTER  LFE    BACK_LEFT  BACK_RIGHT
  // Output: stereo LEFT  1      0      0.707   0.707  0.707      0
  //                RIGHT 0      1      0.707   0.707  0          0.707
  //
  EXPECT_FALSE(remapping);
  EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
  EXPECT_EQ(static_cast<size_t>(input_channels), matrix[LEFT].size());
  EXPECT_EQ(static_cast<size_t>(input_channels), matrix[RIGHT].size());
  EXPECT_EQ(1.0f, matrix[LEFT][LEFT]);
  EXPECT_EQ(1.0f, matrix[RIGHT][RIGHT]);
  EXPECT_EQ(0.0f, matrix[LEFT][RIGHT]);
  EXPECT_EQ(0.0f, matrix[RIGHT][LEFT]);
  EXPECT_EQ(0.0f, matrix[LEFT][BACK_RIGHT]);
  EXPECT_EQ(0.0f, matrix[RIGHT][BACK_LEFT]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[LEFT][CENTER]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[LEFT][LFE]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[LEFT][BACK_LEFT]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[RIGHT][CENTER]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[RIGHT][LFE]);
  EXPECT_FLOAT_EQ(ChannelMixer::kHalfPower, matrix[RIGHT][BACK_RIGHT]);
}

TEST(ChannelMixingMatrixTest, FiveOneToSevenOne) {
  // Front L, Front R, Front C, LFE, Side L, Side R
  ChannelLayout input_layout = CHANNEL_LAYOUT_5_1;
  // Front L, Front R, Front C, LFE, Side L, Side R, Back L, Back R
  ChannelLayout output_layout = CHANNEL_LAYOUT_7_1;
  const int input_channels = ChannelLayoutToChannelCount(input_layout);
  const int output_channels = ChannelLayoutToChannelCount(output_layout);
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

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
  EXPECT_TRUE(remapping);
  EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
  for (int i = 0; i < output_channels; i++) {
    EXPECT_EQ(static_cast<size_t>(input_channels), matrix[i].size());
    for (int j = 0; j < input_channels; j++) {
      if (i == j) {
        EXPECT_EQ(1.0f, matrix[i][j]);
      } else {
        EXPECT_EQ(0.0f, matrix[i][j]);
      }
    }
  }
}

TEST(ChannelMixingMatrixTest, StereoToFiveOne) {
  ChannelLayout input_layout = CHANNEL_LAYOUT_STEREO;
  ChannelLayout output_layout = CHANNEL_LAYOUT_5_1;
  const int input_channels = ChannelLayoutToChannelCount(input_layout);
  const int output_channels = ChannelLayoutToChannelCount(output_layout);
  ChannelMixingMatrix matrix_builder(input_layout, input_channels,
                                     output_layout, output_channels);
  std::vector<std::vector<float>> matrix;
  bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);

  //                         Input: Stereo
  //                         LEFT   RIGHT
  // Output: 5.1 LEFT        1      0
  //             RIGHT       0      1
  //             CENTER      0      0
  //             LFE         0      0
  //             SIDE_LEFT   0      0
  //             SIDE_RIGHT  0      0
  //
  EXPECT_TRUE(remapping);
  EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
  for (int n = 0; n < output_channels; n++) {
    EXPECT_EQ(static_cast<size_t>(input_channels), matrix[n].size());
    if (n == LEFT) {
      EXPECT_EQ(1.0f, matrix[LEFT][LEFT]);
      EXPECT_EQ(0.0f, matrix[LEFT][RIGHT]);
    } else if (n == RIGHT) {
      EXPECT_EQ(0.0f, matrix[RIGHT][LEFT]);
      EXPECT_EQ(1.0f, matrix[RIGHT][RIGHT]);
    } else {
      EXPECT_EQ(0.0f, matrix[n][LEFT]);
      EXPECT_EQ(0.0f, matrix[n][RIGHT]);
    }
  }
}

TEST(ChannelMixingMatrixTest, DiscreteToDiscrete) {
  const struct {
    int input_channels;
    int output_channels;
  } test_case[] = {
      {2, 2},
      {2, 5},
      {5, 2},
  };

  for (size_t n = 0; n < arraysize(test_case); n++) {
    int input_channels = test_case[n].input_channels;
    int output_channels = test_case[n].output_channels;
    ChannelMixingMatrix matrix_builder(CHANNEL_LAYOUT_DISCRETE, input_channels,
                                       CHANNEL_LAYOUT_DISCRETE,
                                       output_channels);
    std::vector<std::vector<float>> matrix;
    bool remapping = matrix_builder.CreateTransformationMatrix(&matrix);
    EXPECT_TRUE(remapping);
    EXPECT_EQ(static_cast<size_t>(output_channels), matrix.size());
    for (int i = 0; i < output_channels; i++) {
      EXPECT_EQ(static_cast<size_t>(input_channels), matrix[i].size());
      for (int j = 0; j < input_channels; j++) {
        if (i == j) {
          EXPECT_EQ(1.0f, matrix[i][j]);
        } else {
          EXPECT_EQ(0.0f, matrix[i][j]);
        }
      }
    }
  }
}

}  // namespace webrtc
