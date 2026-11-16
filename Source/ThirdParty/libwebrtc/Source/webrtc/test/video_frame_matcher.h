/*
 * Copyright 2026 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VIDEO_FRAME_MATCHER_H_
#define TEST_VIDEO_FRAME_MATCHER_H_

#include "api/video/video_frame.h"
#include "test/gtest.h"

namespace webrtc {

// Returns a gMock Matcher that compares two `VideoFrame` objects for equality.
// This matcher verifies that the Y, U, and V pixel data in the `actual_frame`
// is identical to that in the `expected_frame`.
testing::Matcher<const VideoFrame&> PixelValuesEqual(
    const VideoFrame& expected_frame);

}  // namespace webrtc

#endif  // TEST_VIDEO_FRAME_MATCHER_H_
