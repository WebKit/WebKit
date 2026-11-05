/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/NonEscapableBorrow.h>

#include "Helpers/Test.h"
#include <optional>

// These tests exercise the C++ side of NonEscapableBorrow. The Swift-side guarantee, that a view cannot
// escape the call it is lent to, is enforced by the compiler and so is not testable here.

namespace TestWebKitAPI {

#if ASSERT_ENABLED
#define EXPECTED_BORROW_CRASH "ASSERTION FAILED"
#else
#define EXPECTED_BORROW_CRASH ""
#endif

namespace {

struct Counter : CanBorrow {
    int value { 0 };
};

class CounterView : public NonEscapableBorrow<Counter> {
public:
    int value() const { return borrowed().value; }
    void increment() { ++borrowed().value; }

private:
    friend class NonEscapableBorrowLender<CounterView>;

    explicit CounterView(NonEscapableBorrowLease<Counter>& lease LIFETIME_BOUND)
        : NonEscapableBorrow(lease)
    {
    }
} SWIFT_NONESCAPABLE;

}

TEST(WTF_NonEscapableBorrow, ViewReachesTheLentObject)
{
    Counter counter;
    counter.value = 41;
    {
        NonEscapableBorrowLender<CounterView> lender { counter };
        EXPECT_EQ(lender.view().value(), 41);
        lender.view().increment();
    }
    EXPECT_EQ(counter.value, 42);
}

TEST(WTF_NonEscapableBorrow, CopiesReachTheLentObjectWhileItIsLent)
{
    Counter counter;
    NonEscapableBorrowLender<CounterView> lender { counter };
    CounterView copy = lender.view();
    copy.increment();
    CounterView copyOfCopy = copy;
    copyOfCopy.increment();
    EXPECT_EQ(lender.view().value(), 2);
    EXPECT_EQ(counter.value, 2);
}

TEST(WTF_NonEscapableBorrow, ObjectCanBeLentAgain)
{
    Counter counter;
    {
        NonEscapableBorrowLender<CounterView> lender { counter };
        lender.view().increment();
    }
    {
        NonEscapableBorrowLender<CounterView> lender { counter };
        lender.view().increment();
    }
    EXPECT_EQ(counter.value, 2);
}

TEST(WTF_NonEscapableBorrow, NestedLenders)
{
    Counter counter;
    NonEscapableBorrowLender<CounterView> outer { counter };
    {
        NonEscapableBorrowLender<CounterView> inner { counter };
        inner.view().increment();
    }
    outer.view().increment();
    EXPECT_EQ(counter.value, 2);
}

// A copy that outlives its lender, as one a Swift mistake stashed would, crashes when it is used rather
// than reaching the object. These are RELEASE_ASSERTs, so they fire in release builds too.

TEST(WTF_NonEscapableBorrowDeathTest, CopyUsedAfterTheLendEndsCrashes)
{
    auto shouldCrash = [] {
        Counter counter;
        std::optional<CounterView> stash;
        {
            NonEscapableBorrowLender<CounterView> lender { counter };
            stash.emplace(lender.view());
        }
        stash->increment();
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

TEST(WTF_NonEscapableBorrowDeathTest, CopyOfACopyReadAfterTheLendEndsCrashes)
{
    auto shouldCrash = [] {
        Counter counter;
        std::optional<CounterView> stash;
        {
            NonEscapableBorrowLender<CounterView> lender { counter };
            CounterView copy = lender.view();
            stash.emplace(copy);
        }
        stash->value();
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

// The lender holds a Borrow of the object for as long as it lends it, so CanBorrow catches the object
// being destroyed first. CanBorrow only asserts in builds with assertions enabled.
TEST(WTF_NonEscapableBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(ObjectDestroyedWhileLentCrashes))
{
    auto shouldCrash = [] {
        std::optional<Counter> counter;
        counter.emplace();
        NonEscapableBorrowLender<CounterView> lender { *counter };
        counter.reset();
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

} // namespace TestWebKitAPI
