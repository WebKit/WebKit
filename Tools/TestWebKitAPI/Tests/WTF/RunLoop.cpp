/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

static bool testFinished;
static int count = 100;

TEST(WTF_RunLoop, Deadlock)
{
    WTF::initializeMainThread();

    struct DispatchFromDestructorTester {
        ~DispatchFromDestructorTester() {
            RunLoop::main().dispatch([] {
                if (!(--count))
                    testFinished = true;
            });
        }
    };

    for (int i = 0; i < count; ++i) {
        auto capture = std::make_shared<DispatchFromDestructorTester>();
        RunLoop::main().dispatch([capture] { });
    }

    Util::run(&testFinished);
}

TEST(WTF_RunLoop, NestedInOrder)
{
    WTF::initializeMainThread();

    bool done = false;
    bool didExecuteOuter = false;

    RunLoop::main().dispatch([&done, &didExecuteOuter] {
        RunLoop::main().dispatch([&done, &didExecuteOuter] {
            EXPECT_TRUE(didExecuteOuter);
            done = true;
        });

        Util::run(&done);
    });
    RunLoop::main().dispatch([&didExecuteOuter] {
        didExecuteOuter = true;
    });

    Util::run(&done);
}

TEST(WTF_RunLoop, DispatchCrossThreadWhileNested)
{
    WTF::initializeMainThread();

    bool done = false;

    RunLoop::main().dispatch([&done] {
        Thread::create("DispatchCrossThread", [&done] {
            RunLoop::main().dispatch([&done] {
                done = true;
            });
        });

        Util::run(&done);
    });
    RunLoop::main().dispatch([] { });

    Util::run(&done);
}

TEST(WTF_RunLoop, CallOnMainCrossThreadWhileNested)
{
    WTF::initializeMainThread();

    bool done = false;

    callOnMainThread([&done] {
        Thread::create("CallOnMainCrossThread", [&done] {
            callOnMainThread([&done] {
                done = true;
            });
        });

        Util::run(&done);
    });
    callOnMainThread([] { });

    Util::run(&done);
}

class DerivedOneShotTimer : public RunLoop::Timer<DerivedOneShotTimer> {
public:
    DerivedOneShotTimer(bool& testFinished)
        : RunLoop::Timer<DerivedOneShotTimer>(RunLoop::current(), this, &DerivedOneShotTimer::fired)
        , m_testFinished(testFinished)
    {
    }

    void fired()
    {
        EXPECT_FALSE(isActive());
        EXPECT_EQ(secondsUntilFire(), Seconds(0));

        m_testFinished = true;
        stop();
    }

private:
    bool& m_testFinished;
};


TEST(WTF_RunLoop, OneShotTimer)
{
    WTF::initializeMainThread();

    bool testFinished = false;
    DerivedOneShotTimer timer(testFinished);
    timer.startOneShot(100_ms);
    Util::run(&testFinished);
}

class DerivedRepeatingTimer : public RunLoop::Timer<DerivedRepeatingTimer> {
public:
    DerivedRepeatingTimer(bool& testFinished)
        : RunLoop::Timer<DerivedRepeatingTimer>(RunLoop::current(), this, &DerivedRepeatingTimer::fired)
        , m_testFinished(testFinished)
    {
    }

    void fired()
    {
        EXPECT_TRUE(isActive());

        if (++m_count == 10) {
            m_testFinished = true;
            stop();

            EXPECT_FALSE(isActive());
            EXPECT_EQ(secondsUntilFire(), Seconds(0));
        }
    }

private:
    unsigned m_count { 0 };
    bool& m_testFinished;
};


TEST(WTF_RunLoop, RepeatingTimer)
{
    WTF::initializeMainThread();

    bool testFinished = false;
    DerivedRepeatingTimer timer(testFinished);
    timer.startRepeating(10_ms);
    Util::run(&testFinished);
    ASSERT_FALSE(timer.isActive());
}

TEST(WTF_RunLoop, ManyTimes)
{
    class Counter {
    public:
        void run()
        {
            if (++m_count == 100000) {
                RunLoop::current().stop();
                return;
            }
            RunLoop::current().dispatch([this] {
                run();
            });
        }

    private:
        unsigned m_count { 0 };
    };

    Thread::create("RunLoopManyTimes", [] {
        Counter counter;
        RunLoop::current().dispatch([&counter] {
            counter.run();
        });
        RunLoop::run();
    })->waitForCompletion();
}

TEST(WTF_RunLoop, ThreadTerminationSelfReferenceCleanup)
{
    RefPtr<RunLoop> runLoop;

    Thread::create("RunLoopThreadTerminationSelfReferenceCleanup", [&] {
        runLoop = &RunLoop::current();

        // This stores a RunLoop reference in the dispatch queue that will not be released
        // via the usual dispatch, but should still be released upon thread termination.
        // After that, the observing RefPtr should be the only one holding a reference
        // to the RunLoop object.
        runLoop->dispatch([ref = runLoop.copyRef()] { });
    })->waitForCompletion();

    EXPECT_TRUE(runLoop->hasOneRef());
}

TEST(WTF_RunLoop, CapabilityIsCurrentIsSupported)
{
    WTF::initializeMainThread();
    struct {
        int i WTF_GUARDED_BY_CAPABILITY(RunLoop::main()) { 77 };
    } z;
    assertIsCurrent(RunLoop::main());
    bool result = z.i == 77;
    EXPECT_TRUE(result);
}

#if ASSERT_ENABLED
#define MAYBE_CapabilityIsCurrentNegativeDeathTest CapabilityIsCurrentNegativeDeathTest
#else
#define MAYBE_CapabilityIsCurrentNegativeDeathTest DISABLED_CapabilityIsCurrentNegativeDeathTest
#endif
TEST(WTF_RunLoop, MAYBE_CapabilityIsCurrentNegativeDeathTest)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        Thread::create("CapabilityIsCurrentNegative thread", [&] {
            assertIsCurrent(RunLoop::main()); // This should assert.
        })->waitForCompletion();
    }, "ASSERTION FAILED: runLoop.isCurrent()");
}

TEST(WTF_RunLoop, Create)
{
    RefPtr<RunLoop> runLoop = RunLoop::create("RunLoopCreateTestThread"_s, ThreadType::Unknown);
    Thread* runLoopThread = nullptr;
    {
        BinarySemaphore semaphore;
        runLoop->dispatch([&] {
            runLoopThread = &Thread::current();
            semaphore.signal();
        });
        semaphore.wait();
    }
    {
        Locker threadsLock { Thread::allThreadsLock() };
        EXPECT_TRUE(Thread::allThreads().contains(runLoopThread));
    }

    runLoop->dispatch([] {
        RunLoop::current().stop();
    });
    runLoop = nullptr;
    Util::runFor(.2_s);

    // Expect that RunLoop Thread does not leak.
    {
        Locker threadsLock { Thread::allThreadsLock() };
        EXPECT_FALSE(Thread::allThreads().contains(runLoopThread));
    }
}

} // namespace TestWebKitAPI
