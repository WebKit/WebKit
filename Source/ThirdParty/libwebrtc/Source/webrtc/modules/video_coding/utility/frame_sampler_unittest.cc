/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/frame_sampler.h"

#include <cstdint>

#include "api/make_ref_counted.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "test/gtest.h"

namespace webrtc {

TEST(FrameSampler, SamplesBasedOnRtpTimestamp) {
  FrameSampler sampler(TimeDelta::Millis(1000));

  auto buffer = make_ref_counted<I420Buffer>(320, 240);
  VideoFrame frame =
      VideoFrame::Builder().set_video_frame_buffer(buffer).build();

  frame.set_rtp_timestamp(0);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(45'000);
  EXPECT_FALSE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(90'000 - 3'000);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
}

TEST(FrameSampler, SamplesBasedOnRtpTimestampDeltaLessThanOneSecond) {
  FrameSampler sampler(TimeDelta::Millis(1000));

  auto buffer = make_ref_counted<I420Buffer>(320, 240);
  VideoFrame frame =
      VideoFrame::Builder().set_video_frame_buffer(buffer).build();

  frame.set_rtp_timestamp(0);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(3'000);
  EXPECT_FALSE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(90'000 - 3'000);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
}

TEST(FrameSampler, RtpTimestampWraparound) {
  FrameSampler sampler(TimeDelta::Millis(1000));

  auto buffer = make_ref_counted<I420Buffer>(320, 240);
  VideoFrame frame =
      VideoFrame::Builder().set_video_frame_buffer(buffer).build();

  // RTP timestamp wraps at 2**32.
  frame.set_rtp_timestamp(0xffff'ffff - 3'000);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(41'000);
  EXPECT_FALSE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(86'000);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
}

TEST(FrameSampler, CustomInterval) {
  const TimeDelta kSamplingInterval = TimeDelta::Millis(500);
  const uint32_t kFrameInterval = (kSamplingInterval.ms() * 90) / 2;

  FrameSampler sampler(kSamplingInterval);

  auto buffer = make_ref_counted<I420Buffer>(320, 240);
  VideoFrame frame =
      VideoFrame::Builder().set_video_frame_buffer(buffer).build();

  frame.set_rtp_timestamp(0);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(kFrameInterval - 1);
  EXPECT_FALSE(sampler.ShouldBeSampled(frame));
  frame.set_rtp_timestamp(kFrameInterval * 2);
  EXPECT_TRUE(sampler.ShouldBeSampled(frame));
}

}  // namespace webrtc
