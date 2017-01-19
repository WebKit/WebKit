/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_

#include <string>

#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

class VideoProcessingTest : public ::testing::Test {
 protected:
  VideoProcessingTest();
  virtual void SetUp();
  virtual void TearDown();
  static void SetUpTestCase() {
    Trace::CreateTrace();
    std::string trace_file = webrtc::test::OutputPath() + "VPMTrace.txt";
    ASSERT_EQ(0, Trace::SetTraceFile(trace_file.c_str()));
  }
  static void TearDownTestCase() { Trace::ReturnTrace(); }
  VideoProcessing* vp_;
  FILE* source_file_;
  const int width_;
  const int half_width_;
  const int height_;
  const int size_y_;
  const int size_uv_;
  const size_t frame_length_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_TEST_VIDEO_PROCESSING_UNITTEST_H_
