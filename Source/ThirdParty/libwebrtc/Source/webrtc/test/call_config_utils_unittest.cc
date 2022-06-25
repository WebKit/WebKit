/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_config_utils.h"

#include "call/video_receive_stream.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

TEST(CallConfigUtils, MarshalUnmarshalProcessSameObject) {
  VideoReceiveStream::Config recv_config(nullptr);

  VideoReceiveStream::Decoder decoder;
  decoder.payload_type = 10;
  decoder.video_format.name = "test";
  decoder.video_format.parameters["99"] = "b";
  recv_config.decoders.push_back(decoder);
  recv_config.render_delay_ms = 10;
  recv_config.target_delay_ms = 15;
  recv_config.rtp.remote_ssrc = 100;
  recv_config.rtp.local_ssrc = 101;
  recv_config.rtp.rtcp_mode = RtcpMode::kCompound;
  recv_config.rtp.transport_cc = false;
  recv_config.rtp.lntf.enabled = false;
  recv_config.rtp.nack.rtp_history_ms = 150;
  recv_config.rtp.red_payload_type = 50;
  recv_config.rtp.rtx_ssrc = 1000;
  recv_config.rtp.rtx_associated_payload_types[10] = 10;
  recv_config.rtp.extensions.emplace_back("uri", 128, true);

  VideoReceiveStream::Config unmarshaled_config =
      ParseVideoReceiveStreamJsonConfig(
          nullptr, GenerateVideoReceiveStreamJsonConfig(recv_config));

  EXPECT_EQ(recv_config.decoders[0].payload_type,
            unmarshaled_config.decoders[0].payload_type);
  EXPECT_EQ(recv_config.decoders[0].video_format.name,
            unmarshaled_config.decoders[0].video_format.name);
  EXPECT_EQ(recv_config.decoders[0].video_format.parameters,
            unmarshaled_config.decoders[0].video_format.parameters);
  EXPECT_EQ(recv_config.render_delay_ms, unmarshaled_config.render_delay_ms);
  EXPECT_EQ(recv_config.target_delay_ms, unmarshaled_config.target_delay_ms);
  EXPECT_EQ(recv_config.rtp.remote_ssrc, unmarshaled_config.rtp.remote_ssrc);
  EXPECT_EQ(recv_config.rtp.local_ssrc, unmarshaled_config.rtp.local_ssrc);
  EXPECT_EQ(recv_config.rtp.rtcp_mode, unmarshaled_config.rtp.rtcp_mode);
  EXPECT_EQ(recv_config.rtp.transport_cc, unmarshaled_config.rtp.transport_cc);
  EXPECT_EQ(recv_config.rtp.lntf.enabled, unmarshaled_config.rtp.lntf.enabled);
  EXPECT_EQ(recv_config.rtp.nack.rtp_history_ms,
            unmarshaled_config.rtp.nack.rtp_history_ms);
  EXPECT_EQ(recv_config.rtp.red_payload_type,
            unmarshaled_config.rtp.red_payload_type);
  EXPECT_EQ(recv_config.rtp.rtx_ssrc, unmarshaled_config.rtp.rtx_ssrc);
  EXPECT_EQ(recv_config.rtp.rtx_associated_payload_types,
            unmarshaled_config.rtp.rtx_associated_payload_types);
  EXPECT_EQ(recv_config.rtp.extensions, recv_config.rtp.extensions);
}

}  // namespace test
}  // namespace webrtc
