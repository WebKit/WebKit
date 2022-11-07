/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {
namespace {
class CompletionHandlerTest : public testing::Test {
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
};
}

TEST_F(CompletionHandlerTest, SimpleWorks)
{
    {
        CompletionHandler<void()> ch1;
    }

    bool didCall = false;
    auto makeCallable = [&didCall] {
        return [&didCall] { 
            didCall = true;
        };
    };
    CompletionHandler<void()> ch1 { makeCallable() };
    ch1();
    EXPECT_TRUE(didCall);

    didCall = false;
    CompletionHandler<void()> ch2 { makeCallable(), CompletionHandlerCallThread::MainThread };
    ch2();
    EXPECT_TRUE(didCall);
    didCall = false;
    CompletionHandler<void()> ch2_1 { makeCallable(), mainThreadLike };
    ch2_1();
    EXPECT_TRUE(didCall);

    didCall = false;
    CompletionHandler<void()> ch3 { makeCallable(), CompletionHandlerCallThread::ConstructionThread };
    ch3();
    EXPECT_TRUE(didCall);
    didCall = false;
    CompletionHandler<void()> ch3_1 { makeCallable(), currentThreadLike };
    ch3_1();
    EXPECT_TRUE(didCall);

    didCall = false;
    Thread::create(__FUNCTION__, [&] {
        CompletionHandler<void()> ch6 { makeCallable(), CompletionHandlerCallThread::ConstructionThread };
        ch6();
    })->waitForCompletion();
    EXPECT_TRUE(didCall);

    didCall = false;
    CompletionHandler<void()> ch4 { makeCallable(), CompletionHandlerCallThread::AnyThread };
    ch4();
    EXPECT_TRUE(didCall);
    didCall = false;
    CompletionHandler<void()> ch4_1 { makeCallable(), anyThreadLike };
    ch4_1();
    EXPECT_TRUE(didCall);

    didCall = false;
    CompletionHandler<void()> ch5 { makeCallable(),  CompletionHandlerCallThread::AnyThread };
    Thread::create(__FUNCTION__, [&] {
        ch5();
    })->waitForCompletion();
    EXPECT_TRUE(didCall);
}

TEST_F(CompletionHandlerTest, CalledHandlerCanBeDestroyedOffThread)
{
    bool didCall = false;
    auto makeCallable = [&didCall] {
        return [&didCall] { 
            didCall = true;
        };
    };
    CompletionHandler<void()> ch3 { makeCallable(), CompletionHandlerCallThread::ConstructionThread };
    ch3();
    EXPECT_TRUE(didCall);

    bool didDestroy = false;
    Thread::create(__FUNCTION__, [&] {
        auto ch = WTFMove(ch3);
        didDestroy = true;
    })->waitForCompletion();
    EXPECT_TRUE(didDestroy);
}

TEST_F(CompletionHandlerTest, ConstructWithSpecificThreadLikeAssertion)
{
    CompletionHandler<void()> ch;
    BinarySemaphore s;
    auto t = Thread::create(__FUNCTION__, [&] {
        s.wait();
        ch();
    });
    bool didCall = false;
    ch = { 
        [&didCall] { 
            didCall = true;
        }, t->threadLikeAssertion() 
    };
    s.signal();
    t->waitForCompletion();
    EXPECT_TRUE(didCall);
}

TEST(CompletionHandlerDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(UncalledHandlerAsserts))
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        CompletionHandler<void()> ch { [] { } };
    }, "ASSERTION FAILED: Completion handler should always be called");
}

TEST(CompletionHandlerDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(DefaultHandlerOnThreadAsserts))
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        CompletionHandler<void()> ch1 { [] { } };
        Thread::create(__FUNCTION__, [&] {
            ch1(); // This should assert.
        })->waitForCompletion();
    }, "ASSERTION FAILED: threadLikeAssertion.isCurrent\\(\\)");
}

TEST(CompletionHandlerDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(MainThreadHandlerOnThreadAsserts))
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        CompletionHandler<void()> ch1([] { }, CompletionHandlerCallThread::MainThread);
        Thread::create(__FUNCTION__, [&] {
            ch1(); // This should assert.
        })->waitForCompletion();
    }, "ASSERTION FAILED: threadLikeAssertion.isCurrent\\(\\)");

    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        Thread::create(__FUNCTION__, [&] {
            CompletionHandler<void()> ch1([] { }, CompletionHandlerCallThread::MainThread);
            ch1(); // This should assert
        })->waitForCompletion();
    }, "ASSERTION FAILED: threadLikeAssertion.isCurrent\\(\\)");
}

TEST(CompletionHandlerDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(ConstructionThreadHandlerOnThreadAsserts))
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        CompletionHandler<void()> ch1([] { }, CompletionHandlerCallThread::ConstructionThread);
        Thread::create(__FUNCTION__, [&] {
            ch1(); // This should assert.
        })->waitForCompletion();
    }, "ASSERTION FAILED: threadLikeAssertion.isCurrent\\(\\)");

    ASSERT_DEATH_IF_SUPPORTED({
        WTF::initializeMainThread();
        CompletionHandler<void()> ch1;
        BinarySemaphore s;
        auto t1 = Thread::create(__FUNCTION__, [&] {
            ch1 = CompletionHandler<void()>([] { }, CompletionHandlerCallThread::ConstructionThread);
            s.signal();
            while (true) { }
        });
        s.wait();
        Thread::create(__FUNCTION__, [&] {
            ch1(); // This should assert.
        })->waitForCompletion();
    }, "ASSERTION FAILED: threadLikeAssertion.isCurrent\\(\\)");
}

}
