/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/frame_transformer_factory.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/test/mock_transformable_audio_frame.h"
#include "api/test/mock_transformable_video_frame.h"
#include "api/video/video_frame_metadata.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using testing::Each;
using testing::ElementsAreArray;
using testing::NiceMock;
using testing::Return;

TEST(FrameTransformerFactory, CloneAudioFrame) {
  NiceMock<MockTransformableAudioFrame> original_frame;
  uint8_t data[10];
  std::fill_n(data, 10, 5);
  ArrayView<uint8_t> data_view(data);
  ON_CALL(original_frame, GetData()).WillByDefault(Return(data_view));
  auto cloned_frame = CloneAudioFrame(&original_frame);

  EXPECT_THAT(cloned_frame->GetData(), ElementsAreArray(data));
}

TEST(FrameTransformerFactory, CloneVideoFrame) {
  NiceMock<MockTransformableVideoFrame> original_frame;
  uint8_t data[10];
  std::fill_n(data, 10, 5);
  ArrayView<uint8_t> data_view(data);
  EXPECT_CALL(original_frame, GetData()).WillRepeatedly(Return(data_view));
  VideoFrameMetadata metadata;
  std::vector<uint32_t> csrcs{123, 321};
  // Copy csrcs rather than moving so we can compare in an EXPECT_EQ later.
  metadata.SetCsrcs(csrcs);

  EXPECT_CALL(original_frame, Metadata()).WillRepeatedly(Return(metadata));
  auto cloned_frame = CloneVideoFrame(&original_frame);

  EXPECT_EQ(cloned_frame->GetData().size(), 10u);
  EXPECT_THAT(cloned_frame->GetData(), Each(5u));
  EXPECT_EQ(cloned_frame->Metadata().GetCsrcs(), csrcs);
}

}  // namespace
}  // namespace webrtc
