//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WorkerThread_unittest:
//   Simple tests for the worker thread class.

#include <gtest/gtest.h>
#include <array>

#include "libANGLE/WorkerThread.h"

using namespace angle;

namespace
{

// Tests simple worker pool application.
TEST(WorkerPoolTest, SimpleTask)
{
    class TestTask : public Closure
    {
      public:
        void operator()() override { fired = true; }

        bool fired = false;
    };

    std::array<std::shared_ptr<WorkerThreadPool>, 2> pools = {
        {WorkerThreadPool::Create(false), WorkerThreadPool::Create(true)}};
    for (auto &pool : pools)
    {
        std::array<std::shared_ptr<TestTask>, 4> tasks = {
            {std::make_shared<TestTask>(), std::make_shared<TestTask>(),
             std::make_shared<TestTask>(), std::make_shared<TestTask>()}};
        std::array<std::shared_ptr<WaitableEvent>, 4> waitables = {
            {WorkerThreadPool::PostWorkerTask(pool, tasks[0]),
             WorkerThreadPool::PostWorkerTask(pool, tasks[1]),
             WorkerThreadPool::PostWorkerTask(pool, tasks[2]),
             WorkerThreadPool::PostWorkerTask(pool, tasks[3])}};

        WaitableEvent::WaitMany(&waitables);

        for (const auto &task : tasks)
        {
            EXPECT_TRUE(task->fired);
        }
    }
}

}  // anonymous namespace
