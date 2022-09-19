/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/alignment_mixer.h"

#include <string>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AllOf;
using ::testing::Each;

namespace webrtc {
namespace {
std::string ProduceDebugText(bool initial_silence,
                             bool huge_activity_threshold,
                             bool prefer_first_two_channels,
                             int num_channels,
                             int strongest_ch) {
  rtc::StringBuilder ss;
  ss << ", Initial silence: " << initial_silence;
  ss << ", Huge activity threshold: " << huge_activity_threshold;
  ss << ", Prefer first two channels: " << prefer_first_two_channels;
  ss << ", Number of channels: " << num_channels;
  ss << ", Strongest channel: " << strongest_ch;
  return ss.Release();
}

}  // namespace

TEST(AlignmentMixer, GeneralAdaptiveMode) {
  constexpr int kChannelOffset = 100;
  constexpr int kMaxChannelsToTest = 8;
  constexpr float kStrongestSignalScaling =
      kMaxChannelsToTest * kChannelOffset * 100;

  for (bool initial_silence : {false, true}) {
    for (bool huge_activity_threshold : {false, true}) {
      for (bool prefer_first_two_channels : {false, true}) {
        for (int num_channels = 2; num_channels < 8; ++num_channels) {
          for (int strongest_ch = 0; strongest_ch < num_channels;
               ++strongest_ch) {
            SCOPED_TRACE(ProduceDebugText(
                initial_silence, huge_activity_threshold,
                prefer_first_two_channels, num_channels, strongest_ch));
            const float excitation_limit =
                huge_activity_threshold ? 1000000000.f : 0.001f;
            AlignmentMixer am(num_channels, /*downmix*/ false,
                              /*adaptive_selection*/ true, excitation_limit,
                              prefer_first_two_channels);

            Block x(
                /*num_bands=*/1, num_channels);
            if (initial_silence) {
              std::array<float, kBlockSize> y;
              for (int frame = 0; frame < 10 * kNumBlocksPerSecond; ++frame) {
                am.ProduceOutput(x, y);
              }
            }

            for (int frame = 0; frame < 2 * kNumBlocksPerSecond; ++frame) {
              const auto channel_value = [&](int frame_index,
                                             int channel_index) {
                return static_cast<float>(frame_index +
                                          channel_index * kChannelOffset);
              };

              for (int ch = 0; ch < num_channels; ++ch) {
                float scaling =
                    ch == strongest_ch ? kStrongestSignalScaling : 1.f;
                auto x_ch = x.View(/*band=*/0, ch);
                std::fill(x_ch.begin(), x_ch.end(),
                          channel_value(frame, ch) * scaling);
              }

              std::array<float, kBlockSize> y;
              y.fill(-1.f);
              am.ProduceOutput(x, y);

              if (frame > 1 * kNumBlocksPerSecond) {
                if (!prefer_first_two_channels || huge_activity_threshold) {
                  EXPECT_THAT(y,
                              AllOf(Each(x.View(/*band=*/0, strongest_ch)[0])));
                } else {
                  bool left_or_right_chosen;
                  for (int ch = 0; ch < 2; ++ch) {
                    left_or_right_chosen = true;
                    const auto x_ch = x.View(/*band=*/0, ch);
                    for (size_t k = 0; k < kBlockSize; ++k) {
                      if (y[k] != x_ch[k]) {
                        left_or_right_chosen = false;
                        break;
                      }
                    }
                    if (left_or_right_chosen) {
                      break;
                    }
                  }
                  EXPECT_TRUE(left_or_right_chosen);
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST(AlignmentMixer, DownmixMode) {
  for (int num_channels = 1; num_channels < 8; ++num_channels) {
    AlignmentMixer am(num_channels, /*downmix*/ true,
                      /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                      /*prefer_first_two_channels*/ false);

    Block x(/*num_bands=*/1, num_channels);
    const auto channel_value = [](int frame_index, int channel_index) {
      return static_cast<float>(frame_index + channel_index);
    };
    for (int frame = 0; frame < 10; ++frame) {
      for (int ch = 0; ch < num_channels; ++ch) {
        auto x_ch = x.View(/*band=*/0, ch);
        std::fill(x_ch.begin(), x_ch.end(), channel_value(frame, ch));
      }

      std::array<float, kBlockSize> y;
      y.fill(-1.f);
      am.ProduceOutput(x, y);

      float expected_mixed_value = 0.f;
      for (int ch = 0; ch < num_channels; ++ch) {
        expected_mixed_value += channel_value(frame, ch);
      }
      expected_mixed_value *= 1.f / num_channels;

      EXPECT_THAT(y, AllOf(Each(expected_mixed_value)));
    }
  }
}

TEST(AlignmentMixer, FixedMode) {
  for (int num_channels = 1; num_channels < 8; ++num_channels) {
    AlignmentMixer am(num_channels, /*downmix*/ false,
                      /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                      /*prefer_first_two_channels*/ false);

    Block x(/*num_band=*/1, num_channels);
    const auto channel_value = [](int frame_index, int channel_index) {
      return static_cast<float>(frame_index + channel_index);
    };
    for (int frame = 0; frame < 10; ++frame) {
      for (int ch = 0; ch < num_channels; ++ch) {
        auto x_ch = x.View(/*band=*/0, ch);
        std::fill(x_ch.begin(), x_ch.end(), channel_value(frame, ch));
      }

      std::array<float, kBlockSize> y;
      y.fill(-1.f);
      am.ProduceOutput(x, y);
      EXPECT_THAT(y, AllOf(Each(x.View(/*band=*/0, /*channel=*/0)[0])));
    }
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(AlignmentMixerDeathTest, ZeroNumChannels) {
  EXPECT_DEATH(
      AlignmentMixer(/*num_channels*/ 0, /*downmix*/ false,
                     /*adaptive_selection*/ false, /*excitation_limit*/ 1.f,
                     /*prefer_first_two_channels*/ false);
      , "");
}

TEST(AlignmentMixerDeathTest, IncorrectVariant) {
  EXPECT_DEATH(
      AlignmentMixer(/*num_channels*/ 1, /*downmix*/ true,
                     /*adaptive_selection*/ true, /*excitation_limit*/ 1.f,
                     /*prefer_first_two_channels*/ false);
      , "");
}

#endif

}  // namespace webrtc
