/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/echo_canceller3_config_json.h"
#include "api/audio/echo_canceller3_config.h"
#include "test/gtest.h"

namespace webrtc {

TEST(EchoCanceller3JsonHelpers, ToStringAndParseJson) {
  EchoCanceller3Config cfg;
  cfg.delay.down_sampling_factor = 1u;
  cfg.filter.shadow_initial.length_blocks = 7u;
  cfg.suppressor.normal_tuning.mask_hf.enr_suppress = .5f;
  std::string json_string = Aec3ConfigToJsonString(cfg);
  EchoCanceller3Config cfg_transformed = Aec3ConfigFromJsonString(json_string);

  // Expect unchanged values to remain default.
  EXPECT_EQ(cfg.filter.main.error_floor,
            cfg_transformed.filter.main.error_floor);
  EXPECT_EQ(cfg.ep_strength.default_len,
            cfg_transformed.ep_strength.default_len);
  EXPECT_EQ(cfg.suppressor.normal_tuning.mask_lf.enr_suppress,
            cfg_transformed.suppressor.normal_tuning.mask_lf.enr_suppress);

  // Expect changed values to carry through the transformation.
  EXPECT_EQ(cfg.delay.down_sampling_factor,
            cfg_transformed.delay.down_sampling_factor);
  EXPECT_EQ(cfg.filter.shadow_initial.length_blocks,
            cfg_transformed.filter.shadow_initial.length_blocks);
  EXPECT_EQ(cfg.suppressor.normal_tuning.mask_hf.enr_suppress,
            cfg_transformed.suppressor.normal_tuning.mask_hf.enr_suppress);
}
}  // namespace webrtc
