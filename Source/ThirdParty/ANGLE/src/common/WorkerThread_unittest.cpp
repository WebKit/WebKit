//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WorkerThread_unittest:
//   Simple tests for the worker thread class.

#include <gtest/gtest.h>
#include <array>

#include "common/WorkerThread.h"

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
        {WorkerThreadPool::Create(1, ANGLEPlatformCurrent()),
         WorkerThreadPool::Create(0, ANGLEPlatformCurrent())}};
    for (auto &pool : pools)
    {
        std::array<std::shared_ptr<TestTask>, 4> tasks = {
            {std::make_shared<TestTask>(), std::make_shared<TestTask>(),
             std::make_shared<TestTask>(), std::make_shared<TestTask>()}};
        std::array<std::shared_ptr<WaitableEvent>, 4> waitables = {
            {pool->postWorkerTask(tasks[0]), pool->postWorkerTask(tasks[1]),
             pool->postWorkerTask(tasks[2]), pool->postWorkerTask(tasks[3])}};

        WaitableEvent::WaitMany(&waitables);

        for (const auto &task : tasks)
        {
            EXPECT_TRUE(task->fired);
        }
    }
}

// Tests async worker pool application.
TEST(WorkerPoolTest, AsyncPoolTest)
{
    class TestTask : public Closure
    {
      public:
        void operator()() override { fired = true; }

        bool fired = false;
    };
    constexpr size_t kTaskCount                             = 4;
    std::array<std::shared_ptr<TestTask>, kTaskCount> tasks = {
        {std::make_shared<TestTask>(), std::make_shared<TestTask>(), std::make_shared<TestTask>(),
         std::make_shared<TestTask>()}};
    std::array<std::shared_ptr<WaitableEvent>, kTaskCount> waitables;

    {
        std::shared_ptr<WorkerThreadPool> pool =
            WorkerThreadPool::Create(2, ANGLEPlatformCurrent());

        waitables = {{pool->postWorkerTask(tasks[0]), pool->postWorkerTask(tasks[1]),
                      pool->postWorkerTask(tasks[2]), pool->postWorkerTask(tasks[3])}};
    }

    WaitableEvent::WaitMany(&waitables);

    for (size_t taskIndex = 0; taskIndex < kTaskCount; taskIndex++)
    {
        EXPECT_TRUE(tasks[taskIndex]->fired || !waitables[taskIndex]->isReady());
    }
}

}  // anonymous namespace
