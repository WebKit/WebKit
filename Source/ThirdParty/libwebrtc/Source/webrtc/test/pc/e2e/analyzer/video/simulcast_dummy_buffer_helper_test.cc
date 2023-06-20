/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

uint8_t RandByte(Random& random) {
  return random.Rand(255);
}

VideoFrame CreateRandom2x2VideoFrame(uint16_t id, Random& random) {
  rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(2, 2);

  uint8_t data[6] = {RandByte(random), RandByte(random), RandByte(random),
                     RandByte(random), RandByte(random), RandByte(random)};

  memcpy(buffer->MutableDataY(), data, 2);
  memcpy(buffer->MutableDataY() + buffer->StrideY(), data + 2, 2);
  memcpy(buffer->MutableDataU(), data + 4, 1);
  memcpy(buffer->MutableDataV(), data + 5, 1);

  return VideoFrame::Builder()
      .set_id(id)
      .set_video_frame_buffer(buffer)
      .set_timestamp_us(1)
      .build();
}

TEST(CreateDummyFrameBufferTest, CreatedBufferIsDummy) {
  VideoFrame dummy_frame = VideoFrame::Builder()
                               .set_video_frame_buffer(CreateDummyFrameBuffer())
                               .build();

  EXPECT_TRUE(IsDummyFrame(dummy_frame));
}

TEST(IsDummyFrameTest, NotEveryFrameIsDummy) {
  Random random(/*seed=*/100);
  VideoFrame frame = CreateRandom2x2VideoFrame(1, random);
  EXPECT_FALSE(IsDummyFrame(frame));
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
