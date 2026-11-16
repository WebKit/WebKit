/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TASK_QUEUE_TASK_QUEUE_TEST_H_
#define API_TASK_QUEUE_TASK_QUEUE_TEST_H_

#include <functional>
#include <memory>

#include "api/field_trials_view.h"
#include "api/task_queue/task_queue_factory.h"
#include "test/gtest.h"

#if !defined(WEBRTC_CHROMIUM_BUILD) && defined(__cpp_impl_coroutine) &&  \
    (__cpp_impl_coroutine >= 201902L) && defined(__cpp_lib_coroutine) && \
    (__cpp_lib_coroutine >= 201902L)
// Warning that this is still experimental code and that enabling
// `BUILD_EXPERIMENTAL_TASK_QUEUE_COROUTINE_TESTS` for chromium builds caused
// the link time of `components_unittests` in chromium to grow from ~12 min to
// ~30 min! It likely also caused OOM on some machines during the build process.
#define BUILD_EXPERIMENTAL_TASK_QUEUE_COROUTINE_TESTS 1
#endif  // __cpp_impl_coroutine

namespace webrtc {

// Suite of tests to verify TaskQueue implementation with.
// Example usage:
//
// namespace {
//
// using ::testing::Values;
// using ::webrtc::TaskQueueTest;
//
// std::unique_ptr<TaskQueueFactory> CreateMyFactory();
//
// INSTANTIATE_TEST_SUITE_P(My, TaskQueueTest, Values(CreateMyFactory));
//
// }  // namespace
class TaskQueueTest
    : public ::testing::TestWithParam<std::function<
          std::unique_ptr<TaskQueueFactory>(const FieldTrialsView*)>> {};

}  // namespace webrtc

#endif  // API_TASK_QUEUE_TASK_QUEUE_TEST_H_
