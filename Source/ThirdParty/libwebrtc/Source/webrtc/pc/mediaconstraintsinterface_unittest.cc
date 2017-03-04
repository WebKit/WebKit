/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediaconstraintsinterface.h"

#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/base/gunit.h"

namespace webrtc {

namespace {

// Checks all settings touched by CopyConstraintsIntoRtcConfiguration,
// plus audio_jitter_buffer_max_packets.
bool Matches(const PeerConnectionInterface::RTCConfiguration& a,
             const PeerConnectionInterface::RTCConfiguration& b) {
  return a.disable_ipv6 == b.disable_ipv6 &&
         a.audio_jitter_buffer_max_packets ==
             b.audio_jitter_buffer_max_packets &&
         a.enable_rtp_data_channel == b.enable_rtp_data_channel &&
         a.screencast_min_bitrate == b.screencast_min_bitrate &&
         a.combined_audio_video_bwe == b.combined_audio_video_bwe &&
         a.enable_dtls_srtp == b.enable_dtls_srtp &&
         a.media_config.enable_dscp == b.media_config.enable_dscp &&
         a.media_config.video.enable_cpu_overuse_detection ==
             b.media_config.video.enable_cpu_overuse_detection &&
         a.media_config.video.disable_prerenderer_smoothing ==
             b.media_config.video.disable_prerenderer_smoothing &&
         a.media_config.video.suspend_below_min_bitrate ==
             b.media_config.video.suspend_below_min_bitrate;
}

TEST(MediaConstraintsInterface, CopyConstraintsIntoRtcConfiguration) {
  FakeConstraints constraints;
  PeerConnectionInterface::RTCConfiguration old_configuration;
  PeerConnectionInterface::RTCConfiguration configuration;

  CopyConstraintsIntoRtcConfiguration(&constraints, &configuration);
  EXPECT_TRUE(Matches(old_configuration, configuration));

  constraints.SetMandatory(MediaConstraintsInterface::kEnableIPv6, "true");
  CopyConstraintsIntoRtcConfiguration(&constraints, &configuration);
  EXPECT_FALSE(configuration.disable_ipv6);
  constraints.SetMandatory(MediaConstraintsInterface::kEnableIPv6, "false");
  CopyConstraintsIntoRtcConfiguration(&constraints, &configuration);
  EXPECT_TRUE(configuration.disable_ipv6);

  constraints.SetMandatory(MediaConstraintsInterface::kScreencastMinBitrate,
                           27);
  CopyConstraintsIntoRtcConfiguration(&constraints, &configuration);
  EXPECT_TRUE(configuration.screencast_min_bitrate);
  EXPECT_EQ(27, *(configuration.screencast_min_bitrate));

  // An empty set of constraints will not overwrite
  // values that are already present.
  constraints = FakeConstraints();
  configuration = old_configuration;
  configuration.enable_dtls_srtp = rtc::Optional<bool>(true);
  configuration.audio_jitter_buffer_max_packets = 34;
  CopyConstraintsIntoRtcConfiguration(&constraints, &configuration);
  EXPECT_EQ(34, configuration.audio_jitter_buffer_max_packets);
  ASSERT_TRUE(configuration.enable_dtls_srtp);
  EXPECT_TRUE(*(configuration.enable_dtls_srtp));
}

}  // namespace

}  // namespace webrtc
