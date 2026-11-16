/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FULL_CODEC_MATRIX_H_
#define PC_TEST_FULL_CODEC_MATRIX_H_

#include <vector>

#include "api/payload_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"

namespace webrtc {
namespace test {

// The codec list that an endpoint with hardware codecs has in practice. It
// uses up most of the 61 dynamic payload types (35-63 and 96-127), which
// leaves applications that add codecs of their own by munging the SDP with
// very little room. The formats are the ones that
// pc/peer_connection_stability_integrationtest.cc records for a tip of tree
// build. Endpoints with hardware H.265 have more formats still, but H.265 is
// only compiled in when rtc_use_h265 is set, so it is left out here.
// See https://issues.webrtc.org/360058654 for what happens when the payload
// types run out.
//
// The first format is VP8 and the last one is AV1 profile 1, which lets tests
// pick a format that is negotiated early and one that is negotiated late
// without spelling the parameters out again.
inline std::vector<SdpVideoFormat> HardwareVideoFormats() {
  return {
      SdpVideoFormat("VP8"),
      SdpVideoFormat("VP9", {{"profile-id", "0"}}),
      SdpVideoFormat("VP9", {{"profile-id", "1"}}),
      SdpVideoFormat("VP9", {{"profile-id", "2"}}),
      SdpVideoFormat("VP9", {{"profile-id", "3"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "1"},
                              {"profile-level-id", "42001f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "0"},
                              {"profile-level-id", "42001f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "1"},
                              {"profile-level-id", "42e01f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "0"},
                              {"profile-level-id", "42e01f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "1"},
                              {"profile-level-id", "4d001f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "0"},
                              {"profile-level-id", "4d001f"}}),
      SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                              {"packetization-mode", "1"},
                              {"profile-level-id", "f4001f"}}),
      SdpVideoFormat("AV1", {{"level-idx", "5"}, {"profile", "0"}}),
      SdpVideoFormat("AV1", {{"level-idx", "5"}, {"profile", "1"}}),
  };
}

// The video formats above, each of them followed by an RTX codec of its own.
// The payload types are handed out in the order in which the payload type
// picker hands them out, so that the list looks like the one of an endpoint
// that has negotiated once already.
inline std::vector<Codec> HardwareVideoCodecs() {
  int next_payload_type = 35;
  auto payload_type = [&next_payload_type] {
    if (next_payload_type == 64) {
      next_payload_type = 96;
    }
    return PayloadType(next_payload_type++);
  };

  std::vector<Codec> codecs;
  for (const SdpVideoFormat& format : HardwareVideoFormats()) {
    Codec codec = CreateVideoCodec(payload_type(), format);
    codecs.push_back(codec);
    codecs.push_back(CreateVideoRtxCodec(payload_type(), codec.id.value()));
  }
  return codecs;
}

}  // namespace test
}  // namespace webrtc

#endif  // PC_TEST_FULL_CODEC_MATRIX_H_
