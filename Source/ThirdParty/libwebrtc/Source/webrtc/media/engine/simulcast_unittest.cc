/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "media/engine/simulcast.h"
#include "test/gtest.h"

namespace cricket {

class ScreenshareLayerConfigTest : public testing::Test,
                                   protected ScreenshareLayerConfig {
 public:
  ScreenshareLayerConfigTest() : ScreenshareLayerConfig(0, 0) {}

  void ExpectParsingFails(const std::string& group) {
    ScreenshareLayerConfig config(100, 1000);
    EXPECT_FALSE(FromFieldTrialGroup(group, &config));
  }
};

TEST_F(ScreenshareLayerConfigTest, UsesDefaultBitrateConfigForDefaultGroup) {
  ExpectParsingFails("");
}

TEST_F(ScreenshareLayerConfigTest, UsesDefaultConfigForInvalidBitrates) {
  ExpectParsingFails("-");
  ExpectParsingFails("1-");
  ExpectParsingFails("-1");
  ExpectParsingFails("-12");
  ExpectParsingFails("12-");
  ExpectParsingFails("booh!");
  ExpectParsingFails("1-b");
  ExpectParsingFails("a-2");
  ExpectParsingFails("49-1000");
  ExpectParsingFails("50-6001");
  ExpectParsingFails("100-99");
  ExpectParsingFails("1002003004005006-99");
  ExpectParsingFails("99-1002003004005006");
}

TEST_F(ScreenshareLayerConfigTest, ParsesValidBitrateConfig) {
  ScreenshareLayerConfig config(100, 1000);
  EXPECT_TRUE(ScreenshareLayerConfig::FromFieldTrialGroup("101-1001", &config));
  EXPECT_EQ(101, config.tl0_bitrate_kbps);
  EXPECT_EQ(1001, config.tl1_bitrate_kbps);
}

}  // namespace cricket
