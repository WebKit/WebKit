/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/encoded_frame.h"

#include "api/video/video_codec_type.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class TestEncodedFrame : public EncodedFrame {
 public:
  using EncodedFrame::CopyCodecSpecific;
};

TEST(EncodedFrameTest, CopyCodecSpecificH265) {
  TestEncodedFrame frame;
  RTPVideoHeader header;
  header.codec = kVideoCodecH265;

  frame.CopyCodecSpecific(&header);

  EXPECT_EQ(frame.CodecSpecific()->codecType, kVideoCodecH265);
}

}  // namespace
}  // namespace webrtc
