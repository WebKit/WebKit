/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/config_selector.h"

#include <optional>
#include <tuple>

#include "api/audio/echo_canceller3_config.h"
#include "test/gtest.h"

namespace webrtc {

class ConfigSelectorChannelsAndContentDetection
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, bool>> {};

INSTANTIATE_TEST_SUITE_P(ConfigSelectorMultiParameters,
                         ConfigSelectorChannelsAndContentDetection,
                         ::testing::Combine(::testing::Values(1, 2, 8),
                                            ::testing::Values(false, true)));

class ConfigSelectorChannels : public ::testing::Test,
                               public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(ConfigSelectorMultiParameters,
                         ConfigSelectorChannels,
                         ::testing::Values(1, 2, 8));

TEST_P(ConfigSelectorChannelsAndContentDetection,
       MonoConfigIsSelectedWhenNoMultiChannelConfigPresent) {
  const auto [num_channels, detect_stereo_content] = GetParam();
  EchoCanceller3Config config;
  config.multi_channel.detect_stereo_content = detect_stereo_content;
  std::optional<EchoCanceller3Config> multichannel_config;

  config.delay.default_delay = config.delay.default_delay + 1;
  const size_t custom_delay_value_in_config = config.delay.default_delay;

  ConfigSelector cs(config, multichannel_config,
                    /*num_render_input_channels=*/num_channels);
  EXPECT_EQ(cs.active_config().delay.default_delay,
            custom_delay_value_in_config);

  cs.Update(/*multichannel_content=*/false);
  EXPECT_EQ(cs.active_config().delay.default_delay,
            custom_delay_value_in_config);

  cs.Update(/*multichannel_content=*/true);
  EXPECT_EQ(cs.active_config().delay.default_delay,
            custom_delay_value_in_config);
}

TEST_P(ConfigSelectorChannelsAndContentDetection,
       CorrectInitialConfigIsSelected) {
  const auto [num_channels, detect_stereo_content] = GetParam();
  EchoCanceller3Config config;
  config.multi_channel.detect_stereo_content = detect_stereo_content;
  std::optional<EchoCanceller3Config> multichannel_config = config;

  config.delay.default_delay += 1;
  const size_t custom_delay_value_in_config = config.delay.default_delay;
  multichannel_config->delay.default_delay += 2;
  const size_t custom_delay_value_in_multichannel_config =
      multichannel_config->delay.default_delay;

  ConfigSelector cs(config, multichannel_config,
                    /*num_render_input_channels=*/num_channels);

  if (num_channels == 1 || detect_stereo_content) {
    EXPECT_EQ(cs.active_config().delay.default_delay,
              custom_delay_value_in_config);
  } else {
    EXPECT_EQ(cs.active_config().delay.default_delay,
              custom_delay_value_in_multichannel_config);
  }
}

TEST_P(ConfigSelectorChannels, CorrectConfigUpdateBehavior) {
  const int num_channels = GetParam();
  EchoCanceller3Config config;
  config.multi_channel.detect_stereo_content = true;
  std::optional<EchoCanceller3Config> multichannel_config = config;

  config.delay.default_delay += 1;
  const size_t custom_delay_value_in_config = config.delay.default_delay;
  multichannel_config->delay.default_delay += 2;
  const size_t custom_delay_value_in_multichannel_config =
      multichannel_config->delay.default_delay;

  ConfigSelector cs(config, multichannel_config,
                    /*num_render_input_channels=*/num_channels);

  cs.Update(/*multichannel_content=*/false);
  EXPECT_EQ(cs.active_config().delay.default_delay,
            custom_delay_value_in_config);

  if (num_channels == 1) {
    cs.Update(/*multichannel_content=*/false);
    EXPECT_EQ(cs.active_config().delay.default_delay,
              custom_delay_value_in_config);
  } else {
    cs.Update(/*multichannel_content=*/true);
    EXPECT_EQ(cs.active_config().delay.default_delay,
              custom_delay_value_in_multichannel_config);
  }
}

}  // namespace webrtc
