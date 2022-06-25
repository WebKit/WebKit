/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace vp9 {
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;

TEST(Vp9UncompressedHeaderParserTest, FrameWithSegmentation) {
  // Uncompressed header from a frame generated with libvpx.
  // Encoded QVGA frame (SL0 of a VGA frame) that includes a segmentation.
  const uint8_t kHeader[] = {
      0x87, 0x01, 0x00, 0x00, 0x02, 0x7e, 0x01, 0xdf, 0x02, 0x7f, 0x01, 0xdf,
      0xc6, 0x87, 0x04, 0x83, 0x83, 0x2e, 0x46, 0x60, 0x20, 0x38, 0x0c, 0x06,
      0x03, 0xcd, 0x80, 0xc0, 0x60, 0x9f, 0xc5, 0x46, 0x00, 0x00, 0x00, 0x00,
      0x2e, 0x73, 0xb7, 0xee, 0x22, 0x06, 0x81, 0x82, 0xd4, 0xef, 0xc3, 0x58,
      0x1f, 0x12, 0xd2, 0x7b, 0x28, 0x1f, 0x80, 0xfc, 0x07, 0xe0, 0x00, 0x00};

  absl::optional<Vp9UncompressedHeader> frame_info =
      ParseUncompressedVp9Header(kHeader);
  ASSERT_TRUE(frame_info.has_value());

  EXPECT_FALSE(frame_info->is_keyframe);
  EXPECT_TRUE(frame_info->error_resilient);
  EXPECT_TRUE(frame_info->show_frame);
  EXPECT_FALSE(frame_info->show_existing_frame);
  EXPECT_EQ(frame_info->base_qp, 185);
  EXPECT_EQ(frame_info->frame_width, 320);
  EXPECT_EQ(frame_info->frame_height, 240);
  EXPECT_EQ(frame_info->render_width, 640);
  EXPECT_EQ(frame_info->render_height, 480);
  EXPECT_TRUE(frame_info->allow_high_precision_mv);
  EXPECT_EQ(frame_info->frame_context_idx, 0u);
  EXPECT_EQ(frame_info->interpolation_filter,
            Vp9InterpolationFilter::kSwitchable);
  EXPECT_EQ(frame_info->is_lossless, false);
  EXPECT_EQ(frame_info->profile, 0);
  EXPECT_THAT(frame_info->reference_buffers, ElementsAre(0, 0, 0));
  EXPECT_THAT(frame_info->reference_buffers_sign_bias, 0b0000);
  EXPECT_EQ(frame_info->updated_buffers, 0b10000000);
  EXPECT_EQ(frame_info->tile_cols_log2, 0u);
  EXPECT_EQ(frame_info->tile_rows_log2, 0u);
  EXPECT_EQ(frame_info->render_size_offset_bits, 64u);
  EXPECT_EQ(frame_info->compressed_header_size, 23u);
  EXPECT_EQ(frame_info->uncompressed_header_size, 37u);

  EXPECT_TRUE(frame_info->segmentation_enabled);
  EXPECT_FALSE(frame_info->segmentation_is_delta);
  EXPECT_THAT(frame_info->segmentation_pred_prob,
              Optional(ElementsAre(205, 1, 1)));
  EXPECT_THAT(frame_info->segmentation_tree_probs,
              Optional(ElementsAre(255, 255, 128, 1, 128, 128, 128)));
  EXPECT_THAT(frame_info->segmentation_features[1][kVp9SegLvlAlt_Q], Eq(-63));
  EXPECT_THAT(frame_info->segmentation_features[2][kVp9SegLvlAlt_Q], Eq(-81));
}

TEST(Vp9UncompressedHeaderParserTest, SegmentationWithDefaultPredProbs) {
  const uint8_t kHeader[] = {0x90, 0x49, 0x83, 0x42, 0x80, 0x2e,
                             0x30, 0x0,  0xb0, 0x0,  0x37, 0xff,
                             0x06, 0x80, 0x0,  0x0,  0x0,  0x0};
  absl::optional<Vp9UncompressedHeader> frame_info =
      ParseUncompressedVp9Header(kHeader);
  ASSERT_TRUE(frame_info.has_value());
  EXPECT_THAT(frame_info->segmentation_pred_prob,
              Optional(ElementsAre(255, 255, 255)));
}

TEST(Vp9UncompressedHeaderParserTest, SegmentationWithSkipLevel) {
  const uint8_t kHeader[] = {0x90, 0x49, 0x83, 0x42, 0x80, 0x2e, 0x30, 0x00,
                             0xb0, 0x00, 0x37, 0xff, 0x06, 0x80, 0x01, 0x08,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  absl::optional<Vp9UncompressedHeader> frame_info =
      ParseUncompressedVp9Header(kHeader);
  ASSERT_TRUE(frame_info.has_value());
  EXPECT_THAT(frame_info->segmentation_features[0][kVp9SegLvlSkip], Eq(1));
}

}  // namespace vp9
}  // namespace webrtc
