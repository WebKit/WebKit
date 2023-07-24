/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue_stdlib.h"

#include "api/task_queue/task_queue_test.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory(
    const webrtc::FieldTrialsView*) {
  return CreateTaskQueueStdlibFactory();
}

INSTANTIATE_TEST_SUITE_P(TaskQueueStdlib,
                         TaskQueueTest,
                         ::testing::Values(CreateTaskQueueFactory));

}  // namespace
}  // namespace webrtc
