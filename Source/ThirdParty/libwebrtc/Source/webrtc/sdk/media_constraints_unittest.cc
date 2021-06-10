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
  return a.disable_ipv6 == b.disable_ipv6 &&
         a.audio_jitter_buffer_max_packets ==
             b.audio_jitter_buffer_max_packets &&
         a.screencast_min_bitrate == b.screencast_min_bitrate &&
         a.combined_audio_video_bwe == b.combined_audio_video_bwe &&
         a.enable_dtls_srtp == b.enable_dtls_srtp &&
         a.media_config == b.media_config;
}

TEST(MediaConstraints, CopyConstraintsIntoRtcConfiguration) {
  const MediaConstraints constraints_empty;
  PeerConnectionInterface::RTCConfiguration old_configuration;
  PeerConnectionInterface::RTCConfiguration configuration;

  CopyConstraintsIntoRtcConfiguration(&constraints_empty, &configuration);
  EXPECT_TRUE(Matches(old_configuration, configuration));

  const MediaConstraints constraits_enable_ipv6(
      {MediaConstraints::Constraint(MediaConstraints::kEnableIPv6, "true")},
      {});
  CopyConstraintsIntoRtcConfiguration(&constraits_enable_ipv6, &configuration);
  EXPECT_FALSE(configuration.disable_ipv6);
  const MediaConstraints constraints_disable_ipv6(
      {MediaConstraints::Constraint(MediaConstraints::kEnableIPv6, "false")},
      {});
  CopyConstraintsIntoRtcConfiguration(&constraints_disable_ipv6,
                                      &configuration);
  EXPECT_TRUE(configuration.disable_ipv6);

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
  configuration.enable_dtls_srtp = true;
  configuration.audio_jitter_buffer_max_packets = 34;
  CopyConstraintsIntoRtcConfiguration(&constraints_empty, &configuration);
  EXPECT_EQ(34, configuration.audio_jitter_buffer_max_packets);
  ASSERT_TRUE(configuration.enable_dtls_srtp);
  EXPECT_TRUE(*(configuration.enable_dtls_srtp));
}

}  // namespace

}  // namespace webrtc
