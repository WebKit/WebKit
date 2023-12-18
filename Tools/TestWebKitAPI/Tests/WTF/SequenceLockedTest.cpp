/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include <wtf/Scope.h>
#include <wtf/SequenceLocked.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

namespace {
class SequenceLockedTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

struct Tester {
    unsigned a;
    unsigned b;
    unsigned c;
    unsigned d;
};

}

TEST_F(SequenceLockedTest, Works)
{
    SequenceLocked<Tester> tester;
    static_assert(sizeof(tester) - sizeof(uint64_t) == sizeof(Tester));
    std::atomic<bool> done = false;
    auto thread = Thread::create("SequenceLocked test", [&] {
        unsigned i = 0;
        while (!done) {
            tester.store({ i, i, i, i });
            ++i;
        }
    }, ThreadType::Audio, Thread::QOS::UserInteractive);
    auto threadCleanup = makeScopeExit([&] {
        thread->waitForCompletion();
    });
    unsigned maxValue = 0;
    for (int i = 0; i < 100000; ++i) {
        auto t = tester.load();
        EXPECT_EQ(t.a, t.b);
        EXPECT_EQ(t.b, t.c);
        EXPECT_EQ(t.c, t.d);
        EXPECT_LE(maxValue, t.a);
        maxValue = t.a;
    }
    done = true;
}

TEST_F(SequenceLockedTest, DISABLED_WithoutSequenceLockedFails)
{
    Tester tester;
    std::atomic<bool> done = false;
    auto thread = Thread::create("SequenceLocked test", [&] {
        unsigned i = 0;
        while (!done) {
            tester = { i, i, i, i };
            ++i;
        }
    }, ThreadType::Audio, Thread::QOS::UserInteractive);
    auto threadCleanup = makeScopeExit([&] {
        thread->waitForCompletion();
    });
    unsigned maxValue = 0;
    for (int i = 0; i < 10000000; ++i) {
        auto t = tester;
        EXPECT_EQ(t.a, t.b) << "a:" << t.a << " b:" << t.b << " c:" << t.c << " d:" << t.d;
        EXPECT_EQ(t.b, t.c) << "a:" << t.a << " b:" << t.b << " c:" << t.c << " d:" << t.d;
        EXPECT_EQ(t.c, t.d) << "a:" << t.a << " b:" << t.b << " c:" << t.c << " d:" << t.d;
        EXPECT_LE(maxValue, t.a);
        maxValue = t.a;
    }
    done = true;
}

}
