/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/sdp_video_format.h"

#include <stdint.h>

#include "test/gtest.h"

namespace webrtc {

typedef SdpVideoFormat Sdp;
typedef SdpVideoFormat::Parameters Params;

TEST(SdpVideoFormatTest, SameCodecNameNoParameters) {
  EXPECT_TRUE(Sdp("H264").IsSameCodec(Sdp("h264")));
  EXPECT_TRUE(Sdp("VP8").IsSameCodec(Sdp("vp8")));
  EXPECT_TRUE(Sdp("Vp9").IsSameCodec(Sdp("vp9")));
  EXPECT_TRUE(Sdp("AV1").IsSameCodec(Sdp("Av1")));
}
TEST(SdpVideoFormatTest, DifferentCodecNameNoParameters) {
  EXPECT_FALSE(Sdp("H264").IsSameCodec(Sdp("VP8")));
  EXPECT_FALSE(Sdp("VP8").IsSameCodec(Sdp("VP9")));
  EXPECT_FALSE(Sdp("AV1").IsSameCodec(Sdp("")));
}
TEST(SdpVideoFormatTest, SameCodecNameSameParameters) {
  EXPECT_TRUE(Sdp("VP9").IsSameCodec(Sdp("VP9", Params{{"profile-id", "0"}})));
  EXPECT_TRUE(Sdp("VP9", Params{{"profile-id", "0"}})
                  .IsSameCodec(Sdp("VP9", Params{{"profile-id", "0"}})));
  EXPECT_TRUE(Sdp("VP9", Params{{"profile-id", "2"}})
                  .IsSameCodec(Sdp("VP9", Params{{"profile-id", "2"}})));
  EXPECT_TRUE(
      Sdp("H264", Params{{"profile-level-id", "42e01f"}})
          .IsSameCodec(Sdp("H264", Params{{"profile-level-id", "42e01f"}})));
  EXPECT_TRUE(
      Sdp("H264", Params{{"profile-level-id", "640c34"}})
          .IsSameCodec(Sdp("H264", Params{{"profile-level-id", "640c34"}})));
}

TEST(SdpVideoFormatTest, SameCodecNameDifferentParameters) {
  EXPECT_FALSE(Sdp("VP9").IsSameCodec(Sdp("VP9", Params{{"profile-id", "2"}})));
  EXPECT_FALSE(Sdp("VP9", Params{{"profile-id", "0"}})
                   .IsSameCodec(Sdp("VP9", Params{{"profile-id", "1"}})));
  EXPECT_FALSE(Sdp("VP9", Params{{"profile-id", "2"}})
                   .IsSameCodec(Sdp("VP9", Params{{"profile-id", "0"}})));
  EXPECT_FALSE(
      Sdp("H264", Params{{"profile-level-id", "42e01f"}})
          .IsSameCodec(Sdp("H264", Params{{"profile-level-id", "640c34"}})));
  EXPECT_FALSE(
      Sdp("H264", Params{{"profile-level-id", "640c34"}})
          .IsSameCodec(Sdp("H264", Params{{"profile-level-id", "42f00b"}})));
}

TEST(SdpVideoFormatTest, DifferentCodecNameSameParameters) {
  EXPECT_FALSE(Sdp("VP9", Params{{"profile-id", "0"}})
                   .IsSameCodec(Sdp("H264", Params{{"profile-id", "0"}})));
  EXPECT_FALSE(Sdp("VP9", Params{{"profile-id", "2"}})
                   .IsSameCodec(Sdp("VP8", Params{{"profile-id", "2"}})));
  EXPECT_FALSE(
      Sdp("H264", Params{{"profile-level-id", "42e01f"}})
          .IsSameCodec(Sdp("VP9", Params{{"profile-level-id", "42e01f"}})));
  EXPECT_FALSE(
      Sdp("H264", Params{{"profile-level-id", "640c34"}})
          .IsSameCodec(Sdp("VP8", Params{{"profile-level-id", "640c34"}})));
}

}  // namespace webrtc
