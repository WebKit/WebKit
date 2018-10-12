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
#include "rtc_base/criticalsection.h"
#include "test/gtest.h"

namespace webrtc {
TEST(EchoControlMobileTest, InterfaceConfiguration) {
  rtc::CriticalSection crit_render;
  rtc::CriticalSection crit_capture;
  EchoControlMobileImpl aecm(&crit_render, &crit_capture);
  aecm.Initialize(AudioProcessing::kSampleRate16kHz, 2, 2);

  // Turn AECM on
  EXPECT_EQ(0, aecm.Enable(true));
  EXPECT_TRUE(aecm.is_enabled());

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

  // Set and get echo path
  const size_t echo_path_size = aecm.echo_path_size_bytes();
  std::vector<uint8_t> echo_path_in(echo_path_size);
  std::vector<uint8_t> echo_path_out(echo_path_size);
  EXPECT_EQ(AudioProcessing::kNullPointerError,
            aecm.SetEchoPath(nullptr, echo_path_size));
  EXPECT_EQ(AudioProcessing::kNullPointerError,
            aecm.GetEchoPath(nullptr, echo_path_size));
  EXPECT_EQ(AudioProcessing::kBadParameterError,
            aecm.GetEchoPath(echo_path_out.data(), 1));
  EXPECT_EQ(0, aecm.GetEchoPath(echo_path_out.data(), echo_path_size));
  for (size_t i = 0; i < echo_path_size; i++) {
    echo_path_in[i] = echo_path_out[i] + 1;
  }
  EXPECT_EQ(AudioProcessing::kBadParameterError,
            aecm.SetEchoPath(echo_path_in.data(), 1));
  EXPECT_EQ(0, aecm.SetEchoPath(echo_path_in.data(), echo_path_size));
  EXPECT_EQ(0, aecm.GetEchoPath(echo_path_out.data(), echo_path_size));
  for (size_t i = 0; i < echo_path_size; i++) {
    EXPECT_EQ(echo_path_in[i], echo_path_out[i]);
  }

  // Turn AECM off
  EXPECT_EQ(0, aecm.Enable(false));
  EXPECT_FALSE(aecm.is_enabled());
}

}  // namespace webrtc
