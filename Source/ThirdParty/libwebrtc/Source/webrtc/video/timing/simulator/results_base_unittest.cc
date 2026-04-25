/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/results_base.h"

#include <vector>

#include "test/gtest.h"

namespace webrtc::video_timing_simulator {
namespace {

struct TestStream {
  bool fake_is_empty = false;

  bool IsEmpty() const { return fake_is_empty; }
};

struct TestResults : public ResultsBase<TestResults> {
  std::vector<TestStream> streams;
};

TEST(ResultsBaseTest, IsEmptyOnNoStreams) {
  TestResults results;
  EXPECT_TRUE(results.IsEmpty());
}

TEST(ResultsBaseTest, IsEmptyOnAllEmptyStreams) {
  TestResults results{.streams = {TestStream{.fake_is_empty = true},
                                  TestStream{.fake_is_empty = true}}};
  EXPECT_TRUE(results.IsEmpty());
}

TEST(ResultsBaseTest, IsNotEmptyOnSomeNonEmptyStream) {
  TestResults results{.streams = {TestStream{.fake_is_empty = true},
                                  TestStream{.fake_is_empty = false}}};
  EXPECT_FALSE(results.IsEmpty());
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
