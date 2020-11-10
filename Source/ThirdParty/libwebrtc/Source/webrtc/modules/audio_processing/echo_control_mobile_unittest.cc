/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <vector>

#include "modules/audio_processing/echo_control_mobile_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "test/gtest.h"

namespace webrtc {
TEST(EchoControlMobileTest, InterfaceConfiguration) {
  EchoControlMobileImpl aecm;
  aecm.Initialize(AudioProcessing::kSampleRate16kHz, 2, 2);

  // Toggle routing modes
  std::array<EchoControlMobileImpl::RoutingMode, 5> routing_modes = {
      EchoControlMobileImpl::kQuietEarpieceOrHeadset,
      EchoControlMobileImpl::kEarpiece,
      EchoControlMobileImpl::kLoudEarpiece,
      EchoControlMobileImpl::kSpeakerphone,
      EchoControlMobileImpl::kLoudSpeakerphone,
  };
  for (auto mode : routing_modes) {
    EXPECT_EQ(0, aecm.set_routing_mode(mode));
    EXPECT_EQ(mode, aecm.routing_mode());
  }

  // Turn comfort noise off/on
  EXPECT_EQ(0, aecm.enable_comfort_noise(false));
  EXPECT_FALSE(aecm.is_comfort_noise_enabled());
  EXPECT_EQ(0, aecm.enable_comfort_noise(true));
  EXPECT_TRUE(aecm.is_comfort_noise_enabled());
}

}  // namespace webrtc
