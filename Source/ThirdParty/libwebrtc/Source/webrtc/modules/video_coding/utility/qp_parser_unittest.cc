/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/qp_parser.h"

#include <stddef.h>

#include "test/gtest.h"

namespace webrtc {

namespace {
// ffmpeg -s 16x16 -f rawvideo -pix_fmt rgb24 -r 30 -i /dev/zero -c:v libvpx
// -qmin 20 -qmax 20 -crf 20 -frames:v 1 -y out.ivf
const uint8_t kCodedFrameVp8Qp25[] = {
    0x10, 0x02, 0x00, 0x9d, 0x01, 0x2a, 0x10, 0x00, 0x10, 0x00,
    0x02, 0x47, 0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x0c,
    0x82, 0x00, 0x0c, 0x0d, 0x60, 0x00, 0xfe, 0xfc, 0x5c, 0xd0};

// ffmpeg -s 16x16 -f rawvideo -pix_fmt rgb24 -r 30 -i /dev/zero -c:v libvpx-vp9
// -qmin 24 -qmax 24 -crf 24 -frames:v 1 -y out.ivf
const uint8_t kCodedFrameVp9Qp96[] = {
    0xa2, 0x49, 0x83, 0x42, 0xe0, 0x00, 0xf0, 0x00, 0xf6, 0x00,
    0x38, 0x24, 0x1c, 0x18, 0xc0, 0x00, 0x00, 0x30, 0x70, 0x00,
    0x00, 0x4a, 0xa7, 0xff, 0xfc, 0xb9, 0x01, 0xbf, 0xff, 0xff,
    0x97, 0x20, 0xdb, 0xff, 0xff, 0xcb, 0x90, 0x5d, 0x40};

// ffmpeg -s 16x16 -f rawvideo -pix_fmt yuv420p -r 30 -i /dev/zero -c:v libx264
// -qmin 38 -qmax 38 -crf 38 -profile:v baseline -frames:v 2 -y out.264
const uint8_t kCodedFrameH264SpsPpsIdrQp38[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x1e, 0x84,
    0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x3c,
    0x48, 0x99, 0x20, 0x00, 0x00, 0x00, 0x01, 0x68, 0xcb, 0x80, 0xc4,
    0xb2, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0xf1, 0x18, 0xa0, 0x00,
    0x20, 0x5b, 0x1c, 0x00, 0x04, 0x07, 0xe3, 0x80, 0x00, 0x80, 0xfe};

const uint8_t kCodedFrameH264SpsPpsIdrQp49[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x1e, 0x84,
    0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x3c,
    0x48, 0x99, 0x20, 0x00, 0x00, 0x00, 0x01, 0x68, 0xcb, 0x80, 0x5d,
    0x2c, 0x80, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0xf1, 0x18, 0xa0,
    0x00, 0x5e, 0x38, 0x00, 0x08, 0x03, 0xc7, 0x00, 0x01, 0x00, 0x7c};

const uint8_t kCodedFrameH264InterSliceQpDelta0[] = {0x00, 0x00, 0x00, 0x01,
                                                     0x41, 0x9a, 0x39, 0xea};

}  // namespace

TEST(QpParserTest, ParseQpVp8) {
  QpParser parser;
  absl::optional<uint32_t> qp = parser.Parse(
      kVideoCodecVP8, 0, kCodedFrameVp8Qp25, sizeof(kCodedFrameVp8Qp25));
  EXPECT_EQ(qp, 25u);
}

TEST(QpParserTest, ParseQpVp9) {
  QpParser parser;
  absl::optional<uint32_t> qp = parser.Parse(
      kVideoCodecVP9, 0, kCodedFrameVp9Qp96, sizeof(kCodedFrameVp9Qp96));
  EXPECT_EQ(qp, 96u);
}

TEST(QpParserTest, ParseQpH264) {
  QpParser parser;
  absl::optional<uint32_t> qp = parser.Parse(
      VideoCodecType::kVideoCodecH264, 0, kCodedFrameH264SpsPpsIdrQp38,
      sizeof(kCodedFrameH264SpsPpsIdrQp38));
  EXPECT_EQ(qp, 38u);

  qp = parser.Parse(kVideoCodecH264, 1, kCodedFrameH264SpsPpsIdrQp49,
                    sizeof(kCodedFrameH264SpsPpsIdrQp49));
  EXPECT_EQ(qp, 49u);

  qp = parser.Parse(kVideoCodecH264, 0, kCodedFrameH264InterSliceQpDelta0,
                    sizeof(kCodedFrameH264InterSliceQpDelta0));
  EXPECT_EQ(qp, 38u);

  qp = parser.Parse(kVideoCodecH264, 1, kCodedFrameH264InterSliceQpDelta0,
                    sizeof(kCodedFrameH264InterSliceQpDelta0));
  EXPECT_EQ(qp, 49u);
}

TEST(QpParserTest, ParseQpUnsupportedCodecType) {
  QpParser parser;
  absl::optional<uint32_t> qp = parser.Parse(
      kVideoCodecGeneric, 0, kCodedFrameVp8Qp25, sizeof(kCodedFrameVp8Qp25));
  EXPECT_FALSE(qp.has_value());
}

TEST(QpParserTest, ParseQpNullData) {
  QpParser parser;
  absl::optional<uint32_t> qp = parser.Parse(kVideoCodecVP8, 0, nullptr, 100);
  EXPECT_FALSE(qp.has_value());
}

TEST(QpParserTest, ParseQpEmptyData) {
  QpParser parser;
  absl::optional<uint32_t> qp =
      parser.Parse(kVideoCodecVP8, 0, kCodedFrameVp8Qp25, 0);
  EXPECT_FALSE(qp.has_value());
}

TEST(QpParserTest, ParseQpSpatialIdxExceedsMax) {
  QpParser parser;
  absl::optional<uint32_t> qp =
      parser.Parse(kVideoCodecVP8, kMaxSimulcastStreams, kCodedFrameVp8Qp25,
                   sizeof(kCodedFrameVp8Qp25));
  EXPECT_FALSE(qp.has_value());
}

}  // namespace webrtc
