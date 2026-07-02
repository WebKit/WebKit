/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "MoveOnly.h"
#include <wtf/Deque.h>

namespace TestWebKitAPI {

TEST(WTF_Deque, Iterator)
{
    Deque<int> deque;
    deque.append(11);
    deque.prepend(10);
    deque.append(12);
    deque.append(13);

    Deque<int>::iterator it = deque.begin();
    Deque<int>::iterator end = deque.end();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(10, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(13, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF_Deque, InitializerList)
{
    Deque<int> deque = { 1, 2, 3, 4 };

    EXPECT_EQ(4u, deque.size());

    auto it = deque.begin();
    auto end = deque.end();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(1, *it);
    ++it;
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    ++it;
    EXPECT_EQ(4, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF, DequeReverseIterator)
{
    Deque<int> deque;
    deque.append(11);
    deque.prepend(10);
    deque.append(12);
    deque.append(13);

    Deque<int>::reverse_iterator it = deque.rbegin();
    Deque<int>::reverse_iterator end = deque.rend();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(13, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(10, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF_Deque, Remove)
{
    Deque<int> deque;
    deque.append(11);
    deque.prepend(10);
    deque.append(12);
    deque.append(13);

    EXPECT_EQ(10, deque.first());
    EXPECT_EQ(13, deque.last());

    deque.removeLast();
    EXPECT_EQ(10, deque.first());
    EXPECT_EQ(12, deque.last());

    deque.removeFirst();
    EXPECT_EQ(11, deque.first());
    EXPECT_EQ(12, deque.last());

    deque.removeFirst();
    EXPECT_EQ(12, deque.first());
    EXPECT_EQ(12, deque.last());

    deque.removeLast();
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, MoveOnly)
{
    Deque<MoveOnly> deque;

    deque.append(MoveOnly(1));
    deque.prepend(MoveOnly(0));

    EXPECT_EQ(0U, deque.first().value());
    EXPECT_EQ(1U, deque.last().value());

    auto first = deque.takeFirst();
    EXPECT_EQ(0U, first.value());

    auto last = deque.takeLast();
    EXPECT_EQ(1U, last.value());
}

TEST(WTF_Deque, MoveConstructor)
{
    Deque<MoveOnly, 4> deque;

    for (unsigned i = 0; i < 10; ++i)
        deque.append(MoveOnly(i));

    EXPECT_EQ(10u, deque.size());

    Deque<MoveOnly, 4> deque2 = WTF::move(deque);

    EXPECT_EQ(10u, deque2.size());

    unsigned i = 0;
    for (auto& element : deque2) {
        EXPECT_EQ(i, element.value());
        ++i;
    }
}

TEST(WTF_Deque, MoveAssignmentOperator)
{
    Deque<MoveOnly, 4> deque1;

    for (unsigned i = 0; i < 10; ++i)
        deque1.append(MoveOnly(i));

    EXPECT_EQ(10u, deque1.size());

    Deque<MoveOnly, 4> deque2;
    for (unsigned i = 0; i < 10; ++i)
        deque2.append(MoveOnly(i * 2));

    deque1 = WTF::move(deque2);

    EXPECT_EQ(10u, deque2.size());

    unsigned i = 0;
    for (auto& element : deque1) {
        EXPECT_EQ(i * 2, element.value());
        ++i;
    }
}

TEST(WTF_Deque, WrapAroundAppendRemove)
{
    Deque<int> deque;

    // Fill and drain repeatedly to force m_start to advance and wrap around.
    for (int round = 0; round < 5; ++round) {
        for (int i = 0; i < 20; ++i)
            deque.append(round * 100 + i);

        EXPECT_EQ(20u, deque.size());

        for (int i = 0; i < 20; ++i) {
            EXPECT_EQ(round * 100 + i, deque.first());
            deque.removeFirst();
        }

        EXPECT_TRUE(deque.isEmpty());
    }

    // Interleave append/removeFirst to keep the deque small but force wrap.
    for (int i = 0; i < 1000; ++i) {
        deque.append(i);
        if (i % 2 == 0)
            deque.removeFirst();
    }
    // Should have 500 elements remaining (the odd iterations didn't remove).
    EXPECT_EQ(500u, deque.size());
}

TEST(WTF_Deque, WrapAroundExpansion)
{
    Deque<int> deque;

    // Create a wrapped state by appending and removing from front.
    for (int i = 0; i < 10; ++i)
        deque.append(i);
    for (int i = 0; i < 8; ++i)
        deque.removeFirst();
    // Now m_start is advanced. Add more to wrap around and trigger expansion.
    for (int i = 10; i < 30; ++i)
        deque.append(i);

    EXPECT_EQ(22u, deque.size()); // 2 remaining + 20 new

    // Verify all elements are preserved in order.
    for (int i = 0; i < 22; ++i) {
        EXPECT_EQ(8 + i, deque.first());
        deque.removeFirst();
    }
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, InlineCapacityNonPowerOfTwo)
{
    Deque<int, 5> deque; // Should round up to 8 internally.

    for (int i = 0; i < 7; ++i)
        deque.append(i);

    EXPECT_EQ(7u, deque.size());

    // Verify elements.
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ(i, deque.first());
        deque.removeFirst();
    }
    EXPECT_TRUE(deque.isEmpty());

    // Now force expansion beyond inline capacity.
    for (int i = 0; i < 100; ++i)
        deque.append(i);

    EXPECT_EQ(100u, deque.size());

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(i, deque.first());
        deque.removeFirst();
    }
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, StressAppendPrepend)
{
    Deque<int> deque;

    // Interleave appends and prepends, verify ordering.
    for (int i = 0; i < 500; ++i) {
        deque.append(i);
        deque.prepend(-i - 1);
    }

    // Expected: [-500, -499, ..., -1, 0, 1, ..., 499]
    EXPECT_EQ(1000u, deque.size());
    EXPECT_EQ(-500, deque.first());
    EXPECT_EQ(499, deque.last());

    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(-500 + i, deque.first());
        deque.removeFirst();
    }
    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(i, deque.first());
        deque.removeFirst();
    }
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, RemoveAllMatching)
{
    Deque<int> deque = { 1, 2, 3, 4, 5, 6, 7, 8 };

    // Remove even numbers.
    size_t removed = deque.removeAllMatching([](int value) { return !(value % 2); });
    EXPECT_EQ(4u, removed);
    EXPECT_EQ(4u, deque.size());

    // Verify remaining elements and their order.
    auto it = deque.begin();
    EXPECT_EQ(1, *it);
    ++it;
    EXPECT_EQ(3, *it);
    ++it;
    EXPECT_EQ(5, *it);
    ++it;
    EXPECT_EQ(7, *it);
    ++it;
    EXPECT_TRUE(it == deque.end());
}

TEST(WTF_Deque, RemoveAllMatchingNone)
{
    Deque<int> deque = { 1, 2, 3 };

    size_t removed = deque.removeAllMatching([](int) { return false; });
    EXPECT_EQ(0u, removed);
    EXPECT_EQ(3u, deque.size());
}

TEST(WTF_Deque, RemoveAllMatchingAll)
{
    Deque<int> deque = { 1, 2, 3, 4 };

    size_t removed = deque.removeAllMatching([](int) { return true; });
    EXPECT_EQ(4u, removed);
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, RemoveAllMatchingSingleElement)
{
    Deque<int> deque = { 42 };

    size_t removed = deque.removeAllMatching([](int value) { return value == 42; });
    EXPECT_EQ(1u, removed);
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, RemoveFirstMatching)
{
    Deque<int> deque = { 1, 2, 3, 2, 4 };

    bool found = deque.removeFirstMatching([](int value) { return value == 2; });
    EXPECT_TRUE(found);
    EXPECT_EQ(4u, deque.size());

    // Only the first 2 should be removed.
    auto it = deque.begin();
    EXPECT_EQ(1, *it);
    ++it;
    EXPECT_EQ(3, *it);
    ++it;
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(4, *it);

    // No match.
    found = deque.removeFirstMatching([](int value) { return value == 99; });
    EXPECT_FALSE(found);
    EXPECT_EQ(4u, deque.size());
}

TEST(WTF_Deque, TakeFirstWithPredicate)
{
    Deque<int> deque = { 1, 2, 3, 4, 5 };

    // Take the first even number.
    int result = deque.takeFirst([](int value) { return !(value % 2); });
    EXPECT_EQ(2, result);
    EXPECT_EQ(4u, deque.size());

    // Verify remaining elements and order.
    auto it = deque.begin();
    EXPECT_EQ(1, *it); ++it;
    EXPECT_EQ(3, *it); ++it;
    EXPECT_EQ(4, *it); ++it;
    EXPECT_EQ(5, *it); ++it;
    EXPECT_TRUE(it == deque.end());
}

TEST(WTF_Deque, TakeFirstWithPredicateNoMatch)
{
    Deque<int> deque = { 1, 3, 5 };

    int result = deque.takeFirst([](int value) { return !(value % 2); });
    EXPECT_EQ(0, result);
    EXPECT_EQ(3u, deque.size());

    // Verify elements are unchanged.
    auto it = deque.begin();
    EXPECT_EQ(1, *it); ++it;
    EXPECT_EQ(3, *it); ++it;
    EXPECT_EQ(5, *it); ++it;
    EXPECT_TRUE(it == deque.end());
}

TEST(WTF_Deque, TakeFirstWithPredicateFirstElement)
{
    Deque<int> deque = { 10, 1, 2, 3 };

    int result = deque.takeFirst([](int value) { return value == 10; });
    EXPECT_EQ(10, result);
    EXPECT_EQ(3u, deque.size());
    EXPECT_EQ(1, deque.first());
}

TEST(WTF_Deque, TakeFirstWithPredicateLastElement)
{
    Deque<int> deque = { 1, 2, 3, 10 };

    int result = deque.takeFirst([](int value) { return value == 10; });
    EXPECT_EQ(10, result);
    EXPECT_EQ(3u, deque.size());
    EXPECT_EQ(3, deque.last());
}

TEST(WTF_Deque, TakeFirstWithPredicateSingleElement)
{
    Deque<int> deque = { 42 };

    int result = deque.takeFirst([](int value) { return value == 42; });
    EXPECT_EQ(42, result);
    EXPECT_TRUE(deque.isEmpty());
}

TEST(WTF_Deque, TakeLastWithPredicate)
{
    Deque<int> deque = { 1, 2, 3, 4, 5 };

    // Take the last even number (should be 4, not 2).
    int result = deque.takeLast([](int value) { return !(value % 2); });
    EXPECT_EQ(4, result);
    EXPECT_EQ(4u, deque.size());

    // Verify remaining elements and order.
    auto it = deque.begin();
    EXPECT_EQ(1, *it); ++it;
    EXPECT_EQ(2, *it); ++it;
    EXPECT_EQ(3, *it); ++it;
    EXPECT_EQ(5, *it); ++it;
    EXPECT_TRUE(it == deque.end());
}

TEST(WTF_Deque, TakeLastWithPredicateNoMatch)
{
    Deque<int> deque = { 1, 3, 5 };

    int result = deque.takeLast([](int value) { return !(value % 2); });
    EXPECT_EQ(0, result);
    EXPECT_EQ(3u, deque.size());

    // Verify elements are unchanged.
    auto it = deque.begin();
    EXPECT_EQ(1, *it); ++it;
    EXPECT_EQ(3, *it); ++it;
    EXPECT_EQ(5, *it); ++it;
    EXPECT_TRUE(it == deque.end());
}

TEST(WTF_Deque, TakeLastWithPredicateLastElement)
{
    Deque<int> deque = { 1, 2, 3, 10 };

    int result = deque.takeLast([](int value) { return value == 10; });
    EXPECT_EQ(10, result);
    EXPECT_EQ(3u, deque.size());
    EXPECT_EQ(3, deque.last());
}

TEST(WTF_Deque, TakeLastWithPredicateFirstElement)
{
    Deque<int> deque = { 10, 1, 2, 3 };

    int result = deque.takeLast([](int value) { return value == 10; });
    EXPECT_EQ(10, result);
    EXPECT_EQ(3u, deque.size());
    EXPECT_EQ(1, deque.first());
}

TEST(WTF_Deque, TakeFirstWithPredicateWrappedBuffer)
{
    Deque<int> deque;

    // Force wrap-around: append then remove from front to advance m_start.
    for (int i = 0; i < 14; ++i)
        deque.append(i);
    for (int i = 0; i < 12; ++i)
        deque.removeFirst();
    // Now m_start is advanced. Add more so elements wrap around.
    for (int i = 14; i < 26; ++i)
        deque.append(i);

    // Deque contains [12, 13, 14, ..., 25] = 14 elements, wrapping around.
    EXPECT_EQ(14u, deque.size());

    // Take the first element matching > 20 (should be 21, not 12).
    int result = deque.takeFirst([](int value) { return value == 21; });
    EXPECT_EQ(21, result);
    EXPECT_EQ(13u, deque.size());

    // Verify order is preserved.
    int expected = 12;
    for (auto& element : deque) {
        EXPECT_EQ(expected, element);
        expected++;
        if (expected == 21)
            expected = 22; // 21 was removed
    }
}

TEST(WTF_Deque, TakeLastWithPredicateWrappedBuffer)
{
    Deque<int> deque;

    // Force wrap-around.
    for (int i = 0; i < 14; ++i)
        deque.append(i);
    for (int i = 0; i < 12; ++i)
        deque.removeFirst();
    for (int i = 14; i < 26; ++i)
        deque.append(i);

    // Deque contains [12, 13, 14, ..., 25] = 14 elements, wrapping around.
    EXPECT_EQ(14u, deque.size());

    // Take the last even number (should be 24).
    int result = deque.takeLast([](int value) { return !(value % 2); });
    EXPECT_EQ(24, result);
    EXPECT_EQ(13u, deque.size());

    // Verify order is preserved.
    int expected = 12;
    for (auto& element : deque) {
        EXPECT_EQ(expected, element);
        expected++;
        if (expected == 24)
            expected = 25; // 24 was removed
    }
}

} // namespace TestWebKitAPI
