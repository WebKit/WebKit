/*
 * Copyright (C) 2018 Igalia, S.L.
 * Copyright (C) 2018 Metrological Group B.V.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Utilities.h"
#include <WebCore/AbortableTaskQueue.h>
#include <wtf/Threading.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(AbortableTaskQueue, AsyncTasks)
{
    AbortableTaskQueue taskQueue;
    bool testFinished { false };
    int currentStep { 0 };
    RunLoop::initializeMainRunLoop();

    auto backgroundThreadFunction = [&]() {
        EXPECT_FALSE(isMainThread());
        taskQueue.enqueueTask([&]() {
            EXPECT_TRUE(isMainThread());
            currentStep++;
            EXPECT_EQ(1, currentStep);
        });
        taskQueue.enqueueTask([&]() {
            EXPECT_TRUE(isMainThread());
            currentStep++;
            EXPECT_EQ(2, currentStep);
            testFinished = true;
        });
    };
    RunLoop::current().dispatch([backgroundThreadFunction = WTFMove(backgroundThreadFunction)]() mutable {
        WTF::Thread::create("atq-background", WTFMove(backgroundThreadFunction))->detach();
    });

    Util::run(&testFinished);
}

struct FancyResponse {
    WTF_MAKE_NONCOPYABLE(FancyResponse);
public:
    FancyResponse(int fancyInt, bool* destructedFlagPointer = nullptr)
        : fancyInt(fancyInt)
        , destructedFlagPointer(destructedFlagPointer)
    { }

    FancyResponse(FancyResponse&& original)
        : fancyInt(original.fancyInt)
        , destructedFlagPointer(original.destructedFlagPointer)
    {
        original.destructedFlagPointer = nullptr;
    }

    ~FancyResponse()
    {
        if (destructedFlagPointer) {
            RELEASE_ASSERT(!*destructedFlagPointer);
            *destructedFlagPointer = true;
        }
    }

    FancyResponse& operator=(FancyResponse&& a)
    {
        this->fancyInt = a.fancyInt;
        return *this;
    }

    int fancyInt;
    bool* destructedFlagPointer;
};

TEST(AbortableTaskQueue, SyncTasks)
{
    AbortableTaskQueue taskQueue;
    bool testFinished { false };
    bool destructedResponseFlag { false };
    int currentStep { 0 };
    RunLoop::initializeMainRunLoop();

    auto backgroundThreadFunction = [&]() {
        EXPECT_FALSE(isMainThread());
        Optional<FancyResponse> response = taskQueue.enqueueTaskAndWait<FancyResponse>([&]() -> FancyResponse {
            EXPECT_TRUE(isMainThread());
            currentStep++;
            EXPECT_EQ(1, currentStep);
            FancyResponse returnValue(100, &destructedResponseFlag);
            return returnValue;
        });
        currentStep++;
        EXPECT_EQ(2, currentStep);
        EXPECT_TRUE(response);
        EXPECT_FALSE(destructedResponseFlag);
        EXPECT_EQ(100, response->fancyInt);
        response = WTF::nullopt;
        EXPECT_TRUE(destructedResponseFlag);
        RunLoop::main().dispatch([&]() {
            testFinished = true;
        });
    };
    RunLoop::current().dispatch([backgroundThreadFunction = WTFMove(backgroundThreadFunction)]() mutable {
        WTF::Thread::create("atq-background", WTFMove(backgroundThreadFunction))->detach();
    });

    Util::run(&testFinished);
}

template <typename ThreadEnum>
class DeterministicScheduler {
    WTF_MAKE_NONCOPYABLE(DeterministicScheduler);
public:
    DeterministicScheduler(ThreadEnum firstThread)
        : m_currentThread(firstThread)
    { }

    class ThreadContext {
        WTF_MAKE_NONCOPYABLE(ThreadContext);
    public:
        ThreadContext(DeterministicScheduler& scheduler, ThreadEnum thisThread)
            : m_scheduler(scheduler), m_thisThread(thisThread)
        { }

        void waitMyTurn()
        {
            LockHolder lock(m_scheduler.m_mutex);
            m_scheduler.m_currentThreadChanged.wait(m_scheduler.m_mutex, [this]() {
                return m_scheduler.m_currentThread == m_thisThread;
            });
        }

        void yieldToThread(ThreadEnum nextThread)
        {
            LockHolder lock(m_scheduler.m_mutex);
            m_scheduler.m_currentThread = nextThread;
            m_scheduler.m_currentThreadChanged.notifyAll();
        }

    private:
        DeterministicScheduler& m_scheduler;
        ThreadEnum m_thisThread;
    };


private:
    ThreadEnum m_currentThread;
    Condition m_currentThreadChanged;
    Lock m_mutex;
};

enum class TestThread {
    Main,
    Background
};

TEST(AbortableTaskQueue, Abort)
{
    DeterministicScheduler<TestThread> scheduler(TestThread::Background);

    AbortableTaskQueue taskQueue;
    bool testFinished { false };
    RunLoop::initializeMainRunLoop();

    auto backgroundThreadFunction = [&]() {
        EXPECT_FALSE(isMainThread());
        DeterministicScheduler<TestThread>::ThreadContext backgroundThreadContext(scheduler, TestThread::Background);

        taskQueue.enqueueTask([]() {
            // This task should not have been able to run under the scheduling of this test.
            EXPECT_TRUE(false);
        });
        backgroundThreadContext.yieldToThread(TestThread::Main);
        backgroundThreadContext.waitMyTurn();

        // Main thread has called startAborting().

        taskQueue.enqueueTask([]() {
            // This task should not have been able to run under the scheduling of this test.
            EXPECT_TRUE(false);
        });
        // This call must return immediately because we are aborting.
        Optional<FancyResponse> response = taskQueue.enqueueTaskAndWait<FancyResponse>([]() -> FancyResponse {
            // This task should not have been able to run under the scheduling of this test.
            EXPECT_TRUE(false);
            return FancyResponse(100);
        });
        EXPECT_FALSE(response);
        backgroundThreadContext.yieldToThread(TestThread::Main);
        backgroundThreadContext.waitMyTurn();

        // Main thread has called finishAborting().

        taskQueue.enqueueTask([&]() {
            testFinished = true;
        });
    };
    RunLoop::current().dispatch([&, backgroundThreadFunction = WTFMove(backgroundThreadFunction)]() mutable {
        EXPECT_TRUE(isMainThread());
        DeterministicScheduler<TestThread>::ThreadContext mainThreadContext(scheduler, TestThread::Main);
        WTF::Thread::create("atq-background", WTFMove(backgroundThreadFunction))->detach();

        mainThreadContext.waitMyTurn();

        taskQueue.startAborting();
        mainThreadContext.yieldToThread(TestThread::Background);
        mainThreadContext.waitMyTurn();

        taskQueue.finishAborting();
        mainThreadContext.yieldToThread(TestThread::Background);
    });

    Util::run(&testFinished);
}

TEST(AbortableTaskQueue, AbortBeforeSyncTaskRun)
{
    AbortableTaskQueue taskQueue;
    bool testFinished { false };
    RunLoop::initializeMainRunLoop();

    auto backgroundThreadFunction = [&]() {
        EXPECT_FALSE(isMainThread());

        Optional<FancyResponse> response = taskQueue.enqueueTaskAndWait<FancyResponse>([]() -> FancyResponse {
            // This task should not have been able to run under the scheduling of this test.
            EXPECT_TRUE(false);
            return FancyResponse(100);
        });

        // Main thread has called startAborting().
        EXPECT_FALSE(response);

        RunLoop::main().dispatch([&]() {
            testFinished = true;
        });
    };
    RunLoop::current().dispatch([&, backgroundThreadFunction = WTFMove(backgroundThreadFunction)]() mutable {
        EXPECT_TRUE(isMainThread());
        WTF::Thread::create("atq-background", WTFMove(backgroundThreadFunction))->detach();

        // Give the background thread a bit of time to get blocked waiting for a response.
        WTF::sleep(100_ms);

        taskQueue.startAborting();
    });

    Util::run(&testFinished);
}

TEST(AbortableTaskQueue, AbortedBySyncTaskHandler)
{
    AbortableTaskQueue taskQueue;
    bool testFinished { false };
    int currentStep { 0 };
    bool destructedResponseFlag { false };
    RunLoop::initializeMainRunLoop();

    auto backgroundThreadFunction = [&]() {
        EXPECT_FALSE(isMainThread());
        currentStep++;
        EXPECT_EQ(1, currentStep);

        Optional<FancyResponse> response = taskQueue.enqueueTaskAndWait<FancyResponse>([&]() -> FancyResponse {
            currentStep++;
            EXPECT_EQ(2, currentStep);
            taskQueue.startAborting();
            // This task should not have been able to run under the scheduling of this test.
            return FancyResponse(100, &destructedResponseFlag);
        });

        currentStep++;
        EXPECT_EQ(3, currentStep);

        // Main thread has called startAborting().
        EXPECT_FALSE(response);

        // The response object has not been leaked.
        EXPECT_TRUE(destructedResponseFlag);

        RunLoop::main().dispatch([&]() {
            testFinished = true;
        });
    };
    RunLoop::current().dispatch([&, backgroundThreadFunction = WTFMove(backgroundThreadFunction)]() mutable {
        EXPECT_TRUE(isMainThread());
        WTF::Thread::create("atq-background", WTFMove(backgroundThreadFunction))->detach();
    });

    Util::run(&testFinished);
}

} // namespace TestWebKitAPI
