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
        {WorkerThreadPool::Create(ThreadPoolType::Synchronous, 0, ANGLEPlatformCurrent()),
         WorkerThreadPool::Create(ThreadPoolType::Asynchronous, 0, ANGLEPlatformCurrent())}};
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
            WorkerThreadPool::Create(ThreadPoolType::Asynchronous, 2, ANGLEPlatformCurrent());

        waitables = {{pool->postWorkerTask(tasks[0]), pool->postWorkerTask(tasks[1]),
                      pool->postWorkerTask(tasks[2]), pool->postWorkerTask(tasks[3])}};
    }

    WaitableEvent::WaitMany(&waitables);

    for (size_t taskIndex = 0; taskIndex < kTaskCount; taskIndex++)
    {
        EXPECT_TRUE(tasks[taskIndex]->fired || !waitables[taskIndex]->isReady());
    }
}

// Tests async worker pool with a single thread.
TEST(WorkerPoolTest, AsyncPoolWithOneThreadTest)
{
    using CallbackFunc   = std::function<void()>;
    size_t callCount     = 0;
    CallbackFunc counter = [&callCount]() { callCount++; };

    constexpr size_t kCallbackSteps = 1000;
    class TestTask : public Closure
    {
      public:
        TestTask(CallbackFunc callback) : mCallback(callback) { ASSERT(mCallback != nullptr); }
        void operator()() override
        {
            for (size_t iter = 0; iter < kCallbackSteps; iter++)
            {
                mCallback();
            }
        }

      private:
        CallbackFunc mCallback;
    };

    std::shared_ptr<WorkerThreadPool> pool =
        WorkerThreadPool::Create(ThreadPoolType::Asynchronous, 1, ANGLEPlatformCurrent());

    constexpr size_t kTaskCount                             = 4;
    std::array<std::shared_ptr<TestTask>, kTaskCount> tasks = {
        {std::make_shared<TestTask>(counter), std::make_shared<TestTask>(counter),
         std::make_shared<TestTask>(counter), std::make_shared<TestTask>(counter)}};

    std::array<std::shared_ptr<WaitableEvent>, kTaskCount> waitables;
    waitables = {{pool->postWorkerTask(tasks[0]), pool->postWorkerTask(tasks[1]),
                  pool->postWorkerTask(tasks[2]), pool->postWorkerTask(tasks[3])}};

    WaitableEvent::WaitMany(&waitables);

    // Thread pool has 1 thread, all tasks should be serialized
    EXPECT_EQ(callCount, kTaskCount * kCallbackSteps);
}

}  // anonymous namespace
