/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_codec.h"

#include <cstddef>
#include <optional>
#include <vector>

#include "api/video/video_codec_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

VideoCodec CreateVideoCodecForMixedCodec(
    std::optional<VideoCodecType> codec_type,
    std::vector<bool> active_streams,
    std::vector<std::optional<SdpVideoFormat>> formats) {
  RTC_DCHECK(active_streams.size() == formats.size());

  VideoCodec codec;
  if (codec_type) {
    codec.codecType = *codec_type;
  }
  codec.numberOfSimulcastStreams = static_cast<unsigned char>(formats.size());
  for (size_t i = 0; i < formats.size(); ++i) {
    codec.simulcastStream[i].active = active_streams[i];
    codec.simulcastStream[i].format = formats[i];
  }
  return codec;
}

}  // namespace

TEST(VideoCodecTest, TestIsMixedCodec) {
  VideoCodec codec;

  // Non mixed-codec cases
  codec = CreateVideoCodecForMixedCodec(std::nullopt, {}, {});
  EXPECT_FALSE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(std::nullopt, {true},
                                        {SdpVideoFormat::VP8()});
  EXPECT_FALSE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      std::nullopt, {true, true},
      {SdpVideoFormat::VP8(), SdpVideoFormat::VP8()});
  EXPECT_FALSE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      std::nullopt, {true, true, true},
      {SdpVideoFormat::VP8(), SdpVideoFormat::VP8(), SdpVideoFormat::VP8()});
  EXPECT_FALSE(codec.IsMixedCodec());

  // Mixed-codec cases
  codec = CreateVideoCodecForMixedCodec(
      std::nullopt, {true, true},
      {SdpVideoFormat::VP8(), SdpVideoFormat::VP9Profile0()});
  EXPECT_TRUE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      std::nullopt, {true, true},
      {SdpVideoFormat::VP9Profile0(), SdpVideoFormat::VP9Profile1()});
  EXPECT_TRUE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      std::nullopt, {true, true, true},
      {SdpVideoFormat::VP9Profile0(), SdpVideoFormat::VP9Profile1(),
       SdpVideoFormat::VP9Profile0()});
  EXPECT_TRUE(codec.IsMixedCodec());

  // If formats are only partially set, it will never be a mixed-codec
  codec = CreateVideoCodecForMixedCodec(kVideoCodecVP8, {true, true},
                                        {std::nullopt, std::nullopt});
  EXPECT_FALSE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      kVideoCodecVP8, {true, true, true},
      {SdpVideoFormat::VP8(), std::nullopt, SdpVideoFormat::VP9Profile0()});
  EXPECT_FALSE(codec.IsMixedCodec());

  // The format of non-active streams are ignored
  codec = CreateVideoCodecForMixedCodec(
      kVideoCodecVP8, {false, true, true},
      {std::nullopt, SdpVideoFormat::VP8(), SdpVideoFormat::VP9Profile0()});
  EXPECT_TRUE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      kVideoCodecVP9, {true, false, true},
      {SdpVideoFormat::VP8(), std::nullopt, SdpVideoFormat::VP9Profile0()});
  EXPECT_TRUE(codec.IsMixedCodec());

  codec = CreateVideoCodecForMixedCodec(
      kVideoCodecVP8, {true, true, false},
      {SdpVideoFormat::VP8(), SdpVideoFormat::VP8(),
       SdpVideoFormat::VP9Profile0()});
  EXPECT_FALSE(codec.IsMixedCodec());
}

}  // namespace webrtc
