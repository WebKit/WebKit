//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WorkerThread_unittest:
//   Simple tests for the worker thread class.

#include <array>
#include <gtest/gtest.h>

#include "libANGLE/WorkerThread.h"

using namespace angle;

namespace
{

template <typename T>
class WorkerPoolTest : public ::testing::Test
{
  public:
    T workerPool = {4};
};

#if (ANGLE_STD_ASYNC_WORKERS == ANGLE_ENABLED)
using WorkerPoolTypes = ::testing::Types<priv::AsyncWorkerPool, priv::SingleThreadedWorkerPool>;
#else
using WorkerPoolTypes = ::testing::Types<priv::SingleThreadedWorkerPool>;
#endif  // (ANGLE_STD_ASYNC_WORKERS == ANGLE_ENABLED)

TYPED_TEST_CASE(WorkerPoolTest, WorkerPoolTypes);

// Tests simple worker pool application.
TYPED_TEST(WorkerPoolTest, SimpleTask)
{
    class TestTask : public Closure
    {
      public:
        void operator()() override { fired = true; }

        bool fired = false;
    };

    std::array<TestTask, 4> tasks;
    std::array<typename TypeParam::WaitableEventType, 4> waitables = {{
        this->workerPool.postWorkerTask(&tasks[0]), this->workerPool.postWorkerTask(&tasks[1]),
        this->workerPool.postWorkerTask(&tasks[2]), this->workerPool.postWorkerTask(&tasks[3]),
    }};

    TypeParam::WaitableEventType::WaitMany(&waitables);

    for (const auto &task : tasks)
    {
        EXPECT_TRUE(task.fired);
    }
}

}  // anonymous namespace
