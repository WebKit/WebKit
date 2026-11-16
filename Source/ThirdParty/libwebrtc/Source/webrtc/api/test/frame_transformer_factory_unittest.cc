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
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/payload_type.h"
#include "api/test/mock_transformable_audio_frame.h"
#include "api/test/mock_transformable_video_frame.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_metadata.h"
#include "api/video/video_frame_type.h"
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
  std::span<uint8_t> data_view(data);
  ON_CALL(original_frame, GetData()).WillByDefault(Return(data_view));
  auto cloned_frame = CloneAudioFrame(&original_frame);

  EXPECT_THAT(cloned_frame->GetData(), ElementsAreArray(data));
}

TEST(FrameTransformerFactory, CloneVideoFrame) {
  NiceMock<MockTransformableVideoFrame> original_frame;
  uint8_t data[10];
  std::fill_n(data, 10, 5);
  std::span<uint8_t> data_view(data);
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

TEST(FrameTransformerFactory, CreateOutgoingAudioFrame) {
  uint8_t data[] = {1, 2, 3, 4};
  std::vector<uint32_t> csrcs{123, 321};
  auto frame = CreateOutgoingAudioFrame(
      TransformableAudioFrameInterface::FrameType::kAudioFrameSpeech,
      PayloadType(111), /*rtp_timestamp_without_offset=*/222, data,
      sizeof(data), /*absolute_capture_timestamp_ms=*/333, /*ssrc=*/444, csrcs,
      "audio/opus", /*sequence_number=*/555, /*audio_level_dbov=*/67);

  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->Type(),
            TransformableAudioFrameInterface::FrameType::kAudioFrameSpeech);
  EXPECT_EQ(frame->GetPayloadType(), 111u);
  EXPECT_THAT(frame->GetData(), ElementsAreArray(data));
  EXPECT_EQ(frame->AbsoluteCaptureTimestamp(),
            std::make_optional<uint64_t>(333));
  EXPECT_EQ(frame->GetSsrc(), 444u);
  EXPECT_THAT(frame->GetContributingSources(), ElementsAreArray(csrcs));
  EXPECT_EQ(frame->GetMimeType(), "audio/opus");
  EXPECT_EQ(frame->SequenceNumber(), std::make_optional<uint16_t>(555));
  EXPECT_EQ(frame->AudioLevel(), std::make_optional<uint8_t>(67));
  EXPECT_TRUE(std::holds_alternative<RtpTimestampWithoutOffset>(
      frame->GetRtpTimestampInfo()));
  EXPECT_EQ(std::get<RtpTimestampWithoutOffset>(frame->GetRtpTimestampInfo()),
            222u);
}

TEST(FrameTransformerFactory, CreateOutgoingVideoFrame) {
  uint8_t data[] = {1, 2, 3, 4};
  std::vector<uint32_t> csrcs{123, 321};
  auto frame = CreateOutgoingVideoFrame(
      VideoFrameType::kVideoFrameKey, PayloadType(111),
      /*rtp_timestamp_without_offset=*/222, data,
      /*absolute_capture_timestamp_ms=*/333, csrcs,
      VideoCodecType::kVideoCodecVP8,
      /*presentation_timestamp=*/Timestamp::Micros(444),
      /*width=*/640,
      /*height=*/480);

  ASSERT_TRUE(frame);
  EXPECT_TRUE(frame->IsKeyFrame());
  EXPECT_EQ(frame->GetPayloadType(), 111u);
  EXPECT_THAT(frame->GetData(), ElementsAreArray(data));
  EXPECT_EQ(frame->GetSsrc(), 0u);
  EXPECT_STRCASEEQ("video/VP8", frame->GetMimeType().c_str());
  EXPECT_EQ(frame->Metadata().GetWidth(), 640);
  EXPECT_EQ(frame->Metadata().GetHeight(), 480);
  auto rtp_timestamp_info = frame->GetRtpTimestampInfo();
  ASSERT_TRUE(
      std::holds_alternative<RtpTimestampWithoutOffset>(rtp_timestamp_info));
  EXPECT_EQ(std::get<RtpTimestampWithoutOffset>(rtp_timestamp_info).value,
            222u);
  EXPECT_EQ(frame->GetPresentationTimestamp(), Timestamp::Micros(444));
}

}  // namespace
}  // namespace webrtc
