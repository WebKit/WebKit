/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_vps_parser.h"

#include "common_video/h265/h265_common.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {

// Example VPS can be generated with ffmpeg. Here's an example set of commands,
// runnable on Linux:
// 1) Generate a video, from the camera:
// ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720 camera.h265
//
// 2) Open camera.h265 and find the VPS, generally everything between the first
// and second start codes (0 0 0 1 or 0 0 1). The first two bytes should be 0x40
// and 0x01, which should be stripped out before being passed to the parser.

class H265VpsParserTest : public ::testing::Test {
 public:
  H265VpsParserTest() {}
  ~H265VpsParserTest() override {}

  absl::optional<H265VpsParser::VpsState> vps_;
};

TEST_F(H265VpsParserTest, TestSampleVPSId) {
  // VPS id 1
  const uint8_t buffer[] = {
      0x1c, 0x01, 0xff, 0xff, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
      0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x78, 0x95, 0x98, 0x09,
  };
  EXPECT_TRUE(static_cast<bool>(
      vps_ = H265VpsParser::ParseVps(buffer, arraysize(buffer))));
  EXPECT_EQ(1u, vps_->id);
}

}  // namespace webrtc
