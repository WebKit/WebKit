/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/framerate_controller.h"

#include "test/gtest.h"

namespace webrtc {

TEST(FramerateController, KeepTargetFramerate) {
  const float input_framerate_fps = 20;
  const float target_framerate_fps = 5;
  const float max_abs_framerate_error_fps = target_framerate_fps * 0.1f;
  const size_t input_duration_secs = 3;
  const size_t num_input_frames = input_duration_secs * input_framerate_fps;

  FramerateController framerate_controller(target_framerate_fps);
  size_t num_dropped_frames = 0;
  for (size_t frame_num = 0; frame_num < num_input_frames; ++frame_num) {
    const uint32_t timestamp_ms =
        static_cast<uint32_t>(1000 * frame_num / input_framerate_fps);
    if (framerate_controller.DropFrame(timestamp_ms)) {
      ++num_dropped_frames;
    } else {
      framerate_controller.AddFrame(timestamp_ms);
    }
  }

  const float output_framerate_fps =
      static_cast<float>(num_input_frames - num_dropped_frames) /
      input_duration_secs;
  EXPECT_NEAR(output_framerate_fps, target_framerate_fps,
              max_abs_framerate_error_fps);
}

TEST(FramerateController, DoNotDropAnyFramesIfTargerEqualsInput) {
  const float input_framerate_fps = 30;
  const size_t input_duration_secs = 3;
  const size_t num_input_frames = input_duration_secs * input_framerate_fps;

  FramerateController framerate_controller(input_framerate_fps);
  size_t num_dropped_frames = 0;
  for (size_t frame_num = 0; frame_num < num_input_frames; ++frame_num) {
    const uint32_t timestamp_ms =
        static_cast<uint32_t>(1000 * frame_num / input_framerate_fps);
    if (framerate_controller.DropFrame(timestamp_ms)) {
      ++num_dropped_frames;
    } else {
      framerate_controller.AddFrame(timestamp_ms);
    }
  }

  EXPECT_EQ(num_dropped_frames, 0U);
}

TEST(FramerateController, DoNotDropFrameWhenTimestampJumpsBackward) {
  FramerateController framerate_controller(30);
  ASSERT_FALSE(framerate_controller.DropFrame(66));
  framerate_controller.AddFrame(66);
  EXPECT_FALSE(framerate_controller.DropFrame(33));
}

TEST(FramerateController, DropFrameIfItIsTooCloseToPreviousFrame) {
  FramerateController framerate_controller(30);
  ASSERT_FALSE(framerate_controller.DropFrame(33));
  framerate_controller.AddFrame(33);
  EXPECT_TRUE(framerate_controller.DropFrame(34));
}

TEST(FramerateController, FrameDroppingStartsFromSecondInputFrame) {
  const float input_framerate_fps = 23;
  const float target_framerate_fps = 19;
  const uint32_t input_frame_duration_ms =
      static_cast<uint32_t>(1000 / input_framerate_fps);
  FramerateController framerate_controller(target_framerate_fps);
  ASSERT_FALSE(framerate_controller.DropFrame(1 * input_frame_duration_ms));
  framerate_controller.AddFrame(1 * input_frame_duration_ms);
  EXPECT_TRUE(framerate_controller.DropFrame(2 * input_frame_duration_ms));
}

}  // namespace webrtc
