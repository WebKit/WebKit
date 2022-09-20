/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_
#define API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_

#include "absl/functional/any_invocable.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "test/gmock.h"

namespace webrtc {

class MockTaskQueueBase : public TaskQueueBase {
 public:
  MOCK_METHOD(void, Delete, (), (override));
  MOCK_METHOD(void, PostTask, (absl::AnyInvocable<void() &&>), (override));
  MOCK_METHOD(void,
              PostDelayedTask,
              (absl::AnyInvocable<void() &&>, TimeDelta),
              (override));
  MOCK_METHOD(void,
              PostDelayedHighPrecisionTask,
              (absl::AnyInvocable<void() &&>, TimeDelta),
              (override));
};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TEST_MOCK_TASK_QUEUE_BASE_H_
