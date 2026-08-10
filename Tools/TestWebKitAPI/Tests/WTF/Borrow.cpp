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
#include <wtf/Borrow.h>

#include "Helpers/Test.h"
#include <wtf/CanBorrow.h>

namespace TestWebKitAPI {

// Implements the CanBorrow protocol by hand rather than inheriting from CanBorrow,
// so that these tests can observe the borrowed flag instead of only crashing on it.
class ObservableBorrowable {
public:
    bool isBorrowed() const { return m_isBorrowed; }
    unsigned crashIfBorrowedCount() const { return m_crashIfBorrowedCount; }
    int value() const { return m_value; }

    void crashIfBorrowed() const { ++m_crashIfBorrowedCount; }
    bool setIsBorrowed(bool isBorrowed) const { return std::exchange(m_isBorrowed, isBorrowed); }

private:
    int m_value { 42 };
    mutable bool m_isBorrowed { false };
    mutable unsigned m_crashIfBorrowedCount { 0 };
};

class Borrowable : public CanBorrow {
public:
    int value() const { return m_value; }

private:
    int m_value { 7 };
};

TEST(WTF_Borrow, BorrowMarksAndUnmarksTheObject)
{
    ObservableBorrowable object;
    EXPECT_FALSE(object.isBorrowed());
    {
        Borrow borrow(object);
        EXPECT_TRUE(object.isBorrowed());
    }
    EXPECT_FALSE(object.isBorrowed());
}

TEST(WTF_Borrow, BorrowExposesTheObject)
{
    ObservableBorrowable object;
    Borrow borrow(object);

    EXPECT_EQ(&borrow.get(), &object);
    EXPECT_EQ(borrow->value(), 42);
    EXPECT_EQ(&static_cast<ObservableBorrowable&>(borrow), &object);
}

TEST(WTF_Borrow, BorrowHelperFunction)
{
    ObservableBorrowable object;
    EXPECT_EQ(borrow(object)->value(), 42);
    EXPECT_FALSE(object.isBorrowed());
}

// The inner borrow restores the outer borrow's state rather than clearing it.
TEST(WTF_Borrow, NestedBorrowsRestorePreviousState)
{
    ObservableBorrowable object;
    {
        Borrow outer(object);
        {
            Borrow inner(object);
            EXPECT_TRUE(object.isBorrowed());
        }
        EXPECT_TRUE(object.isBorrowed());
    }
    EXPECT_FALSE(object.isBorrowed());
}

TEST(WTF_Borrow, CrashIfBorrowedIsReachedThroughTheProtocol)
{
    ObservableBorrowable object;
    object.crashIfBorrowed();
    EXPECT_EQ(object.crashIfBorrowedCount(), 1u);
}

TEST(WTF_Borrow, DestroyAfterBorrowEndsIsFine)
{
    auto object = makeUniqueWithoutFastMallocCheck<Borrowable>();
    {
        Borrow borrow(*object);
        EXPECT_EQ(borrow->value(), 7);
    }
    object = nullptr;
    EXPECT_FALSE(object);
}

// The CanBorrow assertions are debug-only for now; see the FIXMEs in CanBorrow.h.

TEST(WTF_BorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(DestroyWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        auto* object = new Borrowable;
        Borrow borrow(*object);
        delete object;
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_BorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(CrashIfBorrowedWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        Borrowable object;
        Borrow borrow(object);
        object.crashIfBorrowed();
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

// WTF_FORBID_HEAP_ALLOCATION rules out `new Borrow(...)`, but not embedding a Borrow
// in a heap-allocated object, which would let the borrow outlive the borrowed object.
#if OS(DARWIN)
TEST(WTF_BorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(HeapAllocatedBorrowCrashes))
{
    auto shouldCrash = [] {
        struct Holder {
            explicit Holder(Borrowable& object)
                : borrow(object)
            {
            }
            Borrow<Borrowable> borrow;
        };
        // Both deliberately leaked: the Holder constructor is expected to crash, and
        // nothing else here may be destroyed while the borrow is outstanding, or the
        // test would pass on the wrong assertion.
        SUPPRESS_UNCOUNTED_LOCAL auto* object = new Borrowable;
        SUPPRESS_UNCOUNTED_LOCAL auto* holder = new Holder(*object);
        EXPECT_EQ(holder->borrow->value(), 7);
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}
#endif

} // namespace TestWebKitAPI
