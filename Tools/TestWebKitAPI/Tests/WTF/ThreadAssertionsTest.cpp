/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/ThreadAssertions.h>

#include "Utilities.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace TestWebKitAPI {
namespace {
class MyClass {
public:
    int doTask(int n)
    {
        assertIsCurrent(m_ownerThread);
        return doTaskImpl(n);
    }
    void accessVariable(int n)
    {
        assertIsCurrent(m_ownerThread);
        m_value = n;
    }
    template<typename T> int doTaskCompileFailure(T n) { return doTaskImpl(n); }
    template<typename T> void accessVariableCompileFailure(T n) { m_value = n; }
private:
    int doTaskImpl(int n) WTF_REQUIRES_CAPABILITY(m_ownerThread) { return n + 1; }
    int m_value WTF_GUARDED_BY_CAPABILITY(m_ownerThread) { 0 };
    NO_UNIQUE_ADDRESS ThreadLikeAssertion m_ownerThread;
};
}

TEST(WTF_ThreadAssertions, TestThreadAssertion)
{
    WTF::initializeMainThread();

    MyClass instance;
    EXPECT_EQ(9, instance.doTask(8));
    instance.accessVariable(7);
    // Use following to manually ensure compile will fail:
    // instance.doTaskCompileFailure(9);
    // instance.accessVariableCompileFailure(10);
}

static int testMainThreadFunction() WTF_REQUIRES_CAPABILITY(mainThread) { return 11; }

TEST(WTF_ThreadAssertions, TestMainThreadNamedAssertion)
{
    WTF::initializeMainThread();
    // Use following to manually ensure compile will fail:
    // testMainThreadFunction();
    assertIsMainThread();
    EXPECT_EQ(11, testMainThreadFunction());
}

static int testMainRunLoopFunction() WTF_REQUIRES_CAPABILITY(mainRunLoop) { return 12; }

TEST(WTF_ThreadAssertions, TestMainRunLoopNamedAssertion)
{
    WTF::initializeMainThread();

    bool done = false;
    RunLoop::main().dispatch([&done] {
        // Use following to manually ensure compile will fail:
        // testMainRunLoopFunction();
        assertIsMainRunLoop();
        EXPECT_EQ(12, testMainRunLoopFunction());
        done = true;
    });
    Util::run(&done);
}

} // namespace TestWebKitAPI
