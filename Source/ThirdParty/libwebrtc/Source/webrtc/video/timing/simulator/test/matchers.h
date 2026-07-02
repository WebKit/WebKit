/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_TEST_MATCHERS_H_
#define VIDEO_TIMING_SIMULATOR_TEST_MATCHERS_H_

#include <cstdint>
#include <memory>

#include "api/video/encoded_frame.h"
#include "api/video/video_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc::video_timing_simulator {

inline ::testing::Matcher<const EncodedFrame&> EncodedFrameWithId(
    int64_t expected_id) {
  return ::testing::Property(&EncodedFrame::Id, ::testing::Eq(expected_id));
}
inline ::testing::Matcher<std::unique_ptr<EncodedFrame>> EncodedFramePtrWithId(
    int64_t expected_id) {
  return ::testing::Pointee(EncodedFrameWithId(expected_id));
}
inline ::testing::Matcher<const VideoFrame&> VideoFrameWithId(
    uint16_t expected_id) {
  return ::testing::Property(&VideoFrame::id, ::testing::Eq(expected_id));
}

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_TEST_MATCHERS_H_
