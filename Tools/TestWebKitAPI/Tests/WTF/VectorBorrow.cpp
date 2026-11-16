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
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WTF_VectorBorrow, ReadWhileBorrowedThenInteriorDestroyAfter)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    {
        auto borrowed = borrow(vector);
        EXPECT_EQ(borrowed->size(), 3u);
        EXPECT_EQ(borrowed.get().span().data(), vector.span().data());
    }
    // The borrow has been released, so interior destruction is allowed again.
    vector.reserveCapacity(vector.capacity() + 1024);
    vector.append(4);
    EXPECT_EQ(vector.size(), 4u);
}

TEST(WTF_VectorBorrow, NestedBorrows)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    {
        auto outer = borrow(vector);
        {
            auto inner = borrow(vector);
            EXPECT_EQ(inner->size(), 3u);
        }
        // The inner borrow restored the outer borrow's state, so the Vector is
        // still borrowed here.
        EXPECT_EQ(outer->size(), 3u);
    }
    vector.append(4);
    EXPECT_EQ(vector.size(), 4u);
}

TEST(WTF_VectorBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(ReallocateWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        Vector<uint8_t> vector { 1, 2, 3 };
        auto borrowed = borrow(vector);
        vector.reserveCapacity(vector.capacity() + 1024);
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_VectorBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(DestroyWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        auto* vector = new Vector<uint8_t> { 1, 2, 3 };
        auto borrowed = borrow(*vector);
        delete vector; // Destroys the Vector while the borrow is outstanding.
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_VectorBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(SwapWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        Vector<uint8_t> vector { 1, 2, 3 };
        Vector<uint8_t> other { 4, 5, 6 };
        auto borrowed = borrow(vector);
        vector.swap(other);
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_VectorBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(MoveWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        Vector<uint8_t> vector { 1, 2, 3 };
        auto borrowed = borrow(vector);
        Vector<uint8_t> moved = WTF::move(vector);
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_VectorBorrow, RangeForOverBorrowTemporary)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    unsigned sum = 0;
    for (auto& value : borrow(vector))
        sum += value;
    EXPECT_EQ(sum, 6u);
    // The temporary Borrow ended with the loop, so mutation is allowed again.
    vector.append(4);
    EXPECT_EQ(vector.size(), 4u);
}

TEST(WTF_VectorBorrow, RangeForOverNamedBorrowAllowsMutatingElements)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    Borrow borrowed = vector;
    for (auto& value : borrowed)
        value *= 2;
    EXPECT_EQ(borrowed[0], 2);
    EXPECT_EQ(borrowed[2], 6);
}

TEST(WTF_VectorBorrow, SubscriptThroughBorrow)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    borrow(vector)[1] = 20;
    EXPECT_EQ(vector[1], 20);
    EXPECT_EQ(borrow(vector)[2], 3);

    const Vector<uint8_t> constVector { 4, 5 };
    EXPECT_EQ(borrow(constVector)[1], 5);
    unsigned count = 0;
    for (auto& value : borrow(constVector)) {
        EXPECT_GE(value, 4);
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

TEST(WTF_VectorBorrowDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(MutatingDuringBorrowedRangeForCrashes))
{
    Vector<uint8_t> vector { 1, 2, 3 };
    auto shouldCrash = [&] {
        for (auto& value : borrow(vector)) {
            (void)value;
            vector.append(4);
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

} // namespace TestWebKitAPI
