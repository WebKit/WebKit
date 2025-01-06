/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/media_constraints.h"

#include "test/gtest.h"

namespace webrtc {

namespace {

// Checks all settings touched by CopyConstraintsIntoRtcConfiguration,
// plus audio_jitter_buffer_max_packets.
bool Matches(const PeerConnectionInterface::RTCConfiguration& a,
             const PeerConnectionInterface::RTCConfiguration& b) {
  return a.audio_jitter_buffer_max_packets ==
             b.audio_jitter_buffer_max_packets &&
         a.screencast_min_bitrate == b.screencast_min_bitrate &&
         a.media_config == b.media_config;
}

TEST(MediaConstraints, CopyConstraintsIntoRtcConfiguration) {
  const MediaConstraints constraints_empty;
  PeerConnectionInterface::RTCConfiguration old_configuration;
  PeerConnectionInterface::RTCConfiguration configuration;

  CopyConstraintsIntoRtcConfiguration(&constraints_empty, &configuration);
  EXPECT_TRUE(Matches(old_configuration, configuration));

  const MediaConstraints constraints_screencast(
      {MediaConstraints::Constraint(MediaConstraints::kScreencastMinBitrate,
                                    "27")},
      {});
  CopyConstraintsIntoRtcConfiguration(&constraints_screencast, &configuration);
  EXPECT_TRUE(configuration.screencast_min_bitrate);
  EXPECT_EQ(27, *(configuration.screencast_min_bitrate));

  // An empty set of constraints will not overwrite
  // values that are already present.
  configuration = old_configuration;
  configuration.audio_jitter_buffer_max_packets = 34;
  CopyConstraintsIntoRtcConfiguration(&constraints_empty, &configuration);
  EXPECT_EQ(34, configuration.audio_jitter_buffer_max_packets);
}

}  // namespace

}  // namespace webrtc
