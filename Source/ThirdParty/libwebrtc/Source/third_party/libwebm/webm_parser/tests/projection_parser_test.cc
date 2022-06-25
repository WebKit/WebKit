// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/projection_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::ElementParserTest;
using webm::Id;
using webm::Projection;
using webm::ProjectionParser;
using webm::ProjectionType;

namespace {

class ProjectionParserTest
    : public ElementParserTest<ProjectionParser, Id::kProjection> {};

TEST_F(ProjectionParserTest, DefaultParse) {
  ParseAndVerify();

  const Projection projection = parser_.value();

  EXPECT_FALSE(projection.type.is_present());
  EXPECT_EQ(ProjectionType::kRectangular, projection.type.value());

  EXPECT_FALSE(projection.projection_private.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, projection.projection_private.value());

  EXPECT_FALSE(projection.pose_yaw.is_present());
  EXPECT_EQ(0.0, projection.pose_yaw.value());

  EXPECT_FALSE(projection.pose_pitch.is_present());
  EXPECT_EQ(0.0, projection.pose_pitch.value());

  EXPECT_FALSE(projection.pose_roll.is_present());
  EXPECT_EQ(0.0, projection.pose_roll.value());
}

TEST_F(ProjectionParserTest, DefaultValues) {
  SetReaderData({
      0x76, 0x71,  // ID = 0x7671 (ProjectionType).
      0x80,  // Size = 0.

      0x76, 0x72,  // ID = 0x7672 (ProjectionPrivate).
      0x80,  // Size = 0.

      0x76, 0x73,  // ID = 0x7673 (ProjectionPoseYaw).
      0x80,  // Size = 0.

      0x76, 0x74,  // ID = 0x7674 (ProjectionPosePitch).
      0x80,  // Size = 0.

      0x76, 0x75,  // ID = 0x7675 (ProjectionPoseRoll).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const Projection projection = parser_.value();

  EXPECT_TRUE(projection.type.is_present());
  EXPECT_EQ(ProjectionType::kRectangular, projection.type.value());

  EXPECT_TRUE(projection.projection_private.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{}, projection.projection_private.value());

  EXPECT_TRUE(projection.pose_yaw.is_present());
  EXPECT_EQ(0.0, projection.pose_yaw.value());

  EXPECT_TRUE(projection.pose_pitch.is_present());
  EXPECT_EQ(0.0, projection.pose_pitch.value());

  EXPECT_TRUE(projection.pose_roll.is_present());
  EXPECT_EQ(0.0, projection.pose_roll.value());
}

TEST_F(ProjectionParserTest, CustomValues) {
  SetReaderData({
      0x76, 0x71,  // ID = 0x7671 (ProjectionType).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = equirectangular).

      0x76, 0x72,  // ID = 0x7672 (ProjectionPrivate).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x00,  // Body.

      0x76, 0x73,  // ID = 0x7673 (ProjectionPoseYaw).
      0x10, 0x00, 0x00, 0x04,  // Size = 4.
      0x3f, 0x80, 0x00, 0x00,  // Body (value = 1.0).

      0x76, 0x74,  // ID = 0x7674 (ProjectionPosePitch).
      0x10, 0x00, 0x00, 0x04,  // Size = 4.
      0x40, 0x00, 0x00, 0x00,  // Body (value = 2.0).

      0x76, 0x75,  // ID = 0x7675 (ProjectionPoseRoll).
      0x10, 0x00, 0x00, 0x04,  // Size = 4.
      0x40, 0x80, 0x00, 0x00,  // Body (value = 4.0).
  });

  ParseAndVerify();

  const Projection projection = parser_.value();

  EXPECT_TRUE(projection.type.is_present());
  EXPECT_EQ(ProjectionType::kEquirectangular, projection.type.value());

  EXPECT_TRUE(projection.projection_private.is_present());
  EXPECT_EQ(std::vector<std::uint8_t>{0x00},
            projection.projection_private.value());

  EXPECT_TRUE(projection.pose_yaw.is_present());
  EXPECT_EQ(1.0, projection.pose_yaw.value());

  EXPECT_TRUE(projection.pose_pitch.is_present());
  EXPECT_EQ(2.0, projection.pose_pitch.value());

  EXPECT_TRUE(projection.pose_roll.is_present());
  EXPECT_EQ(4.0, projection.pose_roll.value());
}

TEST_F(ProjectionParserTest, MeshProjection) {
  SetReaderData({
      0x76, 0x71,  // ID = 0x7671 (ProjectionType).
      0x81,  // Size = 1.
      0x03,  // Body (value = mesh).
  });

  ParseAndVerify();

  const Projection projection = parser_.value();

  EXPECT_TRUE(projection.type.is_present());
  EXPECT_EQ(ProjectionType::kMesh, projection.type.value());
}

}  // namespace
