/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "call/rtp_bitrate_configurator.h"

#include <memory>
#include <optional>

#include "api/transport/bitrate_settings.h"
#include "test/gtest.h"

namespace webrtc {
using std::nullopt;

class RtpBitrateConfiguratorTest : public ::testing::Test {
 public:
  RtpBitrateConfiguratorTest()
      : configurator_(new RtpBitrateConfigurator(BitrateConstraints())) {}
  std::unique_ptr<RtpBitrateConfigurator> configurator_;
  void UpdateConfigMatches(BitrateConstraints bitrate_config,
                           std::optional<int> min_bitrate_bps,
                           std::optional<int> start_bitrate_bps,
                           std::optional<int> max_bitrate_bps) {
    std::optional<BitrateConstraints> result =
        configurator_->UpdateWithSdpParameters(bitrate_config);
    EXPECT_TRUE(result.has_value());
    if (start_bitrate_bps.has_value())
      EXPECT_EQ(result->start_bitrate_bps, start_bitrate_bps);
    if (min_bitrate_bps.has_value())
      EXPECT_EQ(result->min_bitrate_bps, min_bitrate_bps);
    if (max_bitrate_bps.has_value())
      EXPECT_EQ(result->max_bitrate_bps, max_bitrate_bps);
  }

  void UpdateMaskMatches(BitrateSettings bitrate_mask,
                         std::optional<int> min_bitrate_bps,
                         std::optional<int> start_bitrate_bps,
                         std::optional<int> max_bitrate_bps) {
    std::optional<BitrateConstraints> result =
        configurator_->UpdateWithClientPreferences(bitrate_mask);
    EXPECT_TRUE(result.has_value());
    if (start_bitrate_bps.has_value())
      EXPECT_EQ(result->start_bitrate_bps, start_bitrate_bps);
    if (min_bitrate_bps.has_value())
      EXPECT_EQ(result->min_bitrate_bps, min_bitrate_bps);
    if (max_bitrate_bps.has_value())
      EXPECT_EQ(result->max_bitrate_bps, max_bitrate_bps);
  }
};

TEST_F(RtpBitrateConfiguratorTest, NewConfigWithValidConfigReturnsNewConfig) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  UpdateConfigMatches(bitrate_config, 1, 2, 3);
}

TEST_F(RtpBitrateConfiguratorTest, NewConfigWithDifferentMinReturnsNewConfig) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  bitrate_config.min_bitrate_bps = 11;
  UpdateConfigMatches(bitrate_config, 11, -1, 30);
}

TEST_F(RtpBitrateConfiguratorTest,
       NewConfigWithDifferentStartReturnsNewConfig) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  bitrate_config.start_bitrate_bps = 21;
  UpdateConfigMatches(bitrate_config, 10, 21, 30);
}

TEST_F(RtpBitrateConfiguratorTest, NewConfigWithDifferentMaxReturnsNewConfig) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  bitrate_config.max_bitrate_bps = 31;
  UpdateConfigMatches(bitrate_config, 10, -1, 31);
}

TEST_F(RtpBitrateConfiguratorTest, NewConfigWithSameConfigElidesSecondCall) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  UpdateConfigMatches(bitrate_config, 1, 2, 3);
  EXPECT_FALSE(
      configurator_->UpdateWithSdpParameters(bitrate_config).has_value());
}

TEST_F(RtpBitrateConfiguratorTest,
       NewConfigWithSameMinMaxAndNegativeStartElidesSecondCall) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  UpdateConfigMatches(bitrate_config, 1, 2, 3);

  bitrate_config.start_bitrate_bps = -1;
  EXPECT_FALSE(
      configurator_->UpdateWithSdpParameters(bitrate_config).has_value());
}

TEST_F(RtpBitrateConfiguratorTest, BiggerMaskMinUsed) {
  BitrateSettings mask;
  mask.min_bitrate_bps = 1234;
  UpdateMaskMatches(mask, *mask.min_bitrate_bps, nullopt, nullopt);
}

TEST_F(RtpBitrateConfiguratorTest, BiggerConfigMinUsed) {
  BitrateSettings mask;
  mask.min_bitrate_bps = 1000;
  UpdateMaskMatches(mask, 1000, nullopt, nullopt);

  BitrateConstraints config;
  config.min_bitrate_bps = 1234;
  UpdateConfigMatches(config, 1234, nullopt, nullopt);
}

// The last call to set start should be used.
TEST_F(RtpBitrateConfiguratorTest, LatestStartMaskPreferred) {
  BitrateSettings mask;
  mask.start_bitrate_bps = 1300;
  UpdateMaskMatches(mask, nullopt, *mask.start_bitrate_bps, nullopt);

  BitrateConstraints bitrate_config;
  bitrate_config.start_bitrate_bps = 1200;

  UpdateConfigMatches(bitrate_config, nullopt, bitrate_config.start_bitrate_bps,
                      nullopt);
}

TEST_F(RtpBitrateConfiguratorTest, SmallerMaskMaxUsed) {
  BitrateConstraints bitrate_config;
  bitrate_config.max_bitrate_bps = bitrate_config.start_bitrate_bps + 2000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  BitrateSettings mask;
  mask.max_bitrate_bps = bitrate_config.start_bitrate_bps + 1000;

  UpdateMaskMatches(mask, nullopt, nullopt, *mask.max_bitrate_bps);
}

TEST_F(RtpBitrateConfiguratorTest, SmallerConfigMaxUsed) {
  BitrateConstraints bitrate_config;
  bitrate_config.max_bitrate_bps = bitrate_config.start_bitrate_bps + 1000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  BitrateSettings mask;
  mask.max_bitrate_bps = bitrate_config.start_bitrate_bps + 2000;

  // Expect no return because nothing changes
  EXPECT_FALSE(configurator_->UpdateWithClientPreferences(mask).has_value());
}

TEST_F(RtpBitrateConfiguratorTest, MaskStartLessThanConfigMinClamped) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 2000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  BitrateSettings mask;
  mask.start_bitrate_bps = 1000;
  UpdateMaskMatches(mask, 2000, 2000, nullopt);
}

TEST_F(RtpBitrateConfiguratorTest, MaskStartGreaterThanConfigMaxClamped) {
  BitrateConstraints bitrate_config;
  bitrate_config.start_bitrate_bps = 2000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  BitrateSettings mask;
  mask.max_bitrate_bps = 1000;

  UpdateMaskMatches(mask, nullopt, -1, 1000);
}

TEST_F(RtpBitrateConfiguratorTest, MaskMinGreaterThanConfigMaxClamped) {
  BitrateConstraints bitrate_config;
  bitrate_config.min_bitrate_bps = 2000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  BitrateSettings mask;
  mask.max_bitrate_bps = 1000;

  UpdateMaskMatches(mask, 1000, nullopt, 1000);
}

TEST_F(RtpBitrateConfiguratorTest, SettingMaskStartForcesUpdate) {
  BitrateSettings mask;
  mask.start_bitrate_bps = 1000;

  // Config should be returned twice with the same params since
  // start_bitrate_bps is set.
  UpdateMaskMatches(mask, nullopt, 1000, nullopt);
  UpdateMaskMatches(mask, nullopt, 1000, nullopt);
}

TEST_F(RtpBitrateConfiguratorTest, NewConfigWithNoChangesDoesNotCallNewConfig) {
  BitrateConstraints config1;
  config1.min_bitrate_bps = 0;
  config1.start_bitrate_bps = 1000;
  config1.max_bitrate_bps = -1;

  BitrateConstraints config2;
  config2.min_bitrate_bps = 0;
  config2.start_bitrate_bps = -1;
  config2.max_bitrate_bps = -1;

  // The second call should not return anything because it doesn't
  // change any values.
  UpdateConfigMatches(config1, 0, 1000, -1);
  EXPECT_FALSE(configurator_->UpdateWithSdpParameters(config2).has_value());
}

// If config changes the max, but not the effective max,
// new config shouldn't be returned, to avoid unnecessary encoder
// reconfigurations.
TEST_F(RtpBitrateConfiguratorTest,
       NewConfigNotReturnedWhenEffectiveMaxUnchanged) {
  BitrateConstraints config;
  config.min_bitrate_bps = 0;
  config.start_bitrate_bps = -1;
  config.max_bitrate_bps = 2000;
  UpdateConfigMatches(config, nullopt, nullopt, 2000);

  // Reduce effective max to 1000 with the mask.
  BitrateSettings mask;
  mask.max_bitrate_bps = 1000;
  UpdateMaskMatches(mask, nullopt, nullopt, 1000);

  // This leaves the effective max unchanged, so new config shouldn't be
  // returned again.
  config.max_bitrate_bps = 1000;
  EXPECT_FALSE(configurator_->UpdateWithSdpParameters(config).has_value());
}

// When the "start bitrate" mask is removed, new config shouldn't be returned
// again, since nothing's changing.
TEST_F(RtpBitrateConfiguratorTest, NewConfigNotReturnedWhenStartMaskRemoved) {
  BitrateSettings mask;
  mask.start_bitrate_bps = 1000;
  UpdateMaskMatches(mask, 0, 1000, -1);

  mask.start_bitrate_bps.reset();
  EXPECT_FALSE(configurator_->UpdateWithClientPreferences(mask).has_value());
}

// Test that if a new config is returned after BitrateSettings applies a
// "start" value, the new config won't return that start value a
// second time.
TEST_F(RtpBitrateConfiguratorTest, NewConfigAfterBitrateConfigMaskWithStart) {
  BitrateSettings mask;
  mask.start_bitrate_bps = 1000;
  UpdateMaskMatches(mask, 0, 1000, -1);

  BitrateConstraints config;
  config.min_bitrate_bps = 0;
  config.start_bitrate_bps = -1;
  config.max_bitrate_bps = 5000;
  // The start value isn't changing, so new config should be returned with
  // -1.
  UpdateConfigMatches(config, 0, -1, 5000);
}

TEST_F(RtpBitrateConfiguratorTest,
       NewConfigNotReturnedWhenClampedMinUnchanged) {
  BitrateConstraints bitrate_config;
  bitrate_config.start_bitrate_bps = 500;
  bitrate_config.max_bitrate_bps = 1000;
  configurator_.reset(new RtpBitrateConfigurator(bitrate_config));

  // Set min to 2000; it is clamped to the max (1000).
  BitrateSettings mask;
  mask.min_bitrate_bps = 2000;
  UpdateMaskMatches(mask, 1000, -1, 1000);

  // Set min to 3000; the clamped value stays the same so nothing happens.
  mask.min_bitrate_bps = 3000;
  EXPECT_FALSE(configurator_->UpdateWithClientPreferences(mask).has_value());
}
}  // namespace webrtc
