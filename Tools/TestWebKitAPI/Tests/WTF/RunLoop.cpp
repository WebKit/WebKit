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

namespace TestWebKitAPI {

static bool testFinished;
static int count = 100;

TEST(WTF_RunLoop, Deadlock)
{
    RunLoop::initializeMainRunLoop();

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

TEST(WTF_RunLoop, NestedRunLoop)
{
    RunLoop::initializeMainRunLoop();

    bool testFinished = false;
    RunLoop::current().dispatch([&] {
        RunLoop::current().dispatch([&] {
            testFinished = true;
        });
        Util::run(&testFinished);
    });

    Util::run(&testFinished);
}

TEST(WTF_RunLoop, OneShotTimer)
{
    RunLoop::initializeMainRunLoop();

    bool testFinished = false;

    class DerivedTimer : public RunLoop::Timer<DerivedTimer> {
    public:
        DerivedTimer(bool& testFinished)
            : RunLoop::Timer<DerivedTimer>(RunLoop::current(), this, &DerivedTimer::fired)
            , m_testFinished(testFinished)
        {
        }

        void fired()
        {
            m_testFinished = true;
            stop();
        }

    private:
        bool& m_testFinished;
    };

    {
        DerivedTimer timer(testFinished);
        timer.startOneShot(100_ms);
        Util::run(&testFinished);
    }
}

TEST(WTF_RunLoop, RepeatingTimer)
{
    RunLoop::initializeMainRunLoop();

    bool testFinished = false;

    class DerivedTimer : public RunLoop::Timer<DerivedTimer> {
    public:
        DerivedTimer(bool& testFinished)
            : RunLoop::Timer<DerivedTimer>(RunLoop::current(), this, &DerivedTimer::fired)
            , m_testFinished(testFinished)
        {
        }

        void fired()
        {
            if (++m_count == 10) {
                m_testFinished = true;
                stop();
            }
        }

    private:
        unsigned m_count { 0 };
        bool& m_testFinished;
    };

    {
        DerivedTimer timer(testFinished);
        timer.startRepeating(10_ms);
        Util::run(&testFinished);
    }
}

TEST(WTF_RunLoop, ManyTimes)
{
    RunLoop::initializeMainRunLoop();

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

    Counter counter;

    RunLoop::current().dispatch([&counter] {
        counter.run();
    });
    RunLoop::run();
}

} // namespace TestWebKitAPI
