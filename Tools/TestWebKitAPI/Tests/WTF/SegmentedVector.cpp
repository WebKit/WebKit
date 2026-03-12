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
#include <wtf/SegmentedVector.h>

#include "MoveOnly.h"

namespace TestWebKitAPI {

struct DestructorCounter {
    static unsigned s_destructorCount;
    static void resetCount() { s_destructorCount = 0; }

    int value;

    DestructorCounter()
        : value(0) { }
    DestructorCounter(int v)
        : value(v) { }
    ~DestructorCounter() { ++s_destructorCount; }

    DestructorCounter(const DestructorCounter& other)
        : value(other.value) { }
    DestructorCounter& operator=(const DestructorCounter& other)
    {
        value = other.value;
        return *this;
    }
};

unsigned DestructorCounter::s_destructorCount = 0;

// ============================================================
// Section 1: Heap-only mode (InlineCapacity = 0)
// ============================================================

TEST(WTF_SegmentedVector, HeapOnlyEmpty)
{
    SegmentedVector<int, 4> vector;
    EXPECT_TRUE(vector.isEmpty());
    EXPECT_EQ(0u, vector.size());
}

TEST(WTF_SegmentedVector, HeapOnlyAppendAndAccess)
{
    SegmentedVector<int, 4> vector;

    vector.append(1);
    vector.append(2);
    vector.append(3);

    EXPECT_FALSE(vector.isEmpty());
    EXPECT_EQ(3u, vector.size());

    EXPECT_EQ(1, vector.at(0));
    EXPECT_EQ(2, vector.at(1));
    EXPECT_EQ(3, vector.at(2));

    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
}

TEST(WTF_SegmentedVector, HeapOnlyFirstAndLast)
{
    SegmentedVector<int, 4> vector;

    vector.append(10);
    vector.append(20);
    vector.append(30);

    EXPECT_EQ(10, vector.first());
    EXPECT_EQ(30, vector.last());
}

TEST(WTF_SegmentedVector, HeapOnlyMultipleSegments)
{
    SegmentedVector<int, 4> vector;

    // 12 elements = 3 segments of 4
    for (int i = 0; i < 12; ++i)
        vector.append(i);

    EXPECT_EQ(12u, vector.size());
    for (int i = 0; i < 12; ++i)
        EXPECT_EQ(i, vector[i]);
}

TEST(WTF_SegmentedVector, HeapOnlyPointerStability)
{
    SegmentedVector<int, 4> vector;

    vector.append(100);
    int* firstPtr = &vector[0];

    // Add more elements, triggering additional segment allocations
    for (int i = 1; i < 20; ++i)
        vector.append(i * 100);

    // Original pointer should still be valid
    EXPECT_EQ(100, *firstPtr);

    // Also verify pointers to later segments remain stable
    int* laterPtr = &vector[10];
    for (int i = 20; i < 40; ++i)
        vector.append(i * 100);

    EXPECT_EQ(1000, *laterPtr);
}

TEST(WTF_SegmentedVector, HeapOnlyRemoveLast)
{
    SegmentedVector<int, 4> vector;

    vector.append(1);
    vector.append(2);
    vector.append(3);

    EXPECT_EQ(3u, vector.size());

    vector.removeLast();
    EXPECT_EQ(2u, vector.size());
    EXPECT_EQ(2, vector.last());

    vector.removeLast();
    EXPECT_EQ(1u, vector.size());
    EXPECT_EQ(1, vector.last());

    vector.removeLast();
    EXPECT_TRUE(vector.isEmpty());
}

TEST(WTF_SegmentedVector, HeapOnlyTakeLast)
{
    SegmentedVector<int, 4> vector;

    vector.append(10);
    vector.append(20);
    vector.append(30);

    EXPECT_EQ(30, vector.takeLast());
    EXPECT_EQ(2u, vector.size());

    EXPECT_EQ(20, vector.takeLast());
    EXPECT_EQ(1u, vector.size());
}

TEST(WTF_SegmentedVector, HeapOnlyClear)
{
    SegmentedVector<int, 4> vector;

    for (int i = 0; i < 20; ++i)
        vector.append(i);

    EXPECT_EQ(20u, vector.size());

    vector.clear();
    EXPECT_TRUE(vector.isEmpty());
    EXPECT_EQ(0u, vector.size());
}

TEST(WTF_SegmentedVector, HeapOnlyGrow)
{
    SegmentedVector<int, 4> vector;

    vector.append(1);
    vector.grow(10);

    EXPECT_EQ(10u, vector.size());
    EXPECT_EQ(1, vector[0]);

    // Default-constructed elements should be 0
    for (size_t i = 1; i < 10; ++i)
        EXPECT_EQ(0, vector[i]);
}

TEST(WTF_SegmentedVector, HeapOnlyShrinkToFit)
{
    SegmentedVector<int, 4> vector;

    for (int i = 0; i < 20; ++i)
        vector.append(i);

    vector.shrinkToFit();

    EXPECT_EQ(20u, vector.size());
    for (int i = 0; i < 20; ++i)
        EXPECT_EQ(i, vector[i]);
}

TEST(WTF_SegmentedVector, HeapOnlyIterator)
{
    SegmentedVector<int, 4> vector;

    vector.append(10);
    vector.append(20);
    vector.append(30);

    auto it = vector.begin();
    auto end = vector.end();

    EXPECT_TRUE(it != end);

    EXPECT_EQ(10, *it);
    ++it;
    EXPECT_EQ(20, *it);
    ++it;
    EXPECT_EQ(30, *it);
    ++it;

    EXPECT_TRUE(it == end);
}

TEST(WTF_SegmentedVector, HeapOnlyIteratorMultipleSegments)
{
    SegmentedVector<int, 4> vector;

    for (int i = 0; i < 12; ++i)
        vector.append(i * 10);

    int expected = 0;
    for (auto& value : vector) {
        EXPECT_EQ(expected, value);
        expected += 10;
    }
    EXPECT_EQ(120, expected);
}

TEST(WTF_SegmentedVector, HeapOnlyAlloc)
{
    SegmentedVector<int, 4> vector;

    int& ref1 = vector.alloc(100);
    EXPECT_EQ(100, ref1);
    EXPECT_EQ(1u, vector.size());

    int& ref2 = vector.alloc(200);
    EXPECT_EQ(200, ref2);
    EXPECT_EQ(2u, vector.size());

    // References should remain valid
    EXPECT_EQ(100, ref1);
    EXPECT_EQ(200, ref2);
}

TEST(WTF_SegmentedVector, HeapOnlyAllocMultipleSegments)
{
    SegmentedVector<int, 4> vector;

    int* refs[12];
    for (int i = 0; i < 12; ++i)
        refs[i] = &vector.alloc(i * 100);

    // All references should remain valid
    for (int i = 0; i < 12; ++i)
        EXPECT_EQ(i * 100, *refs[i]);
}

TEST(WTF_SegmentedVector, HeapOnlyMoveOnly)
{
    SegmentedVector<MoveOnly, 4> vector;

    vector.append(MoveOnly(1));
    vector.append(MoveOnly(2));
    vector.append(MoveOnly(3));

    EXPECT_EQ(1u, vector[0].value());
    EXPECT_EQ(2u, vector[1].value());
    EXPECT_EQ(3u, vector[2].value());

    auto last = vector.takeLast();
    EXPECT_EQ(3u, last.value());
    EXPECT_EQ(2u, vector.size());
}

TEST(WTF_SegmentedVector, HeapOnlyMoveOnlyMultipleSegments)
{
    SegmentedVector<MoveOnly, 4> vector;

    for (unsigned i = 0; i < 12; ++i)
        vector.append(MoveOnly(i));

    EXPECT_EQ(12u, vector.size());

    for (unsigned i = 0; i < 12; ++i)
        EXPECT_EQ(i, vector[i].value());
}

// ============================================================
// Section 2: Inline + Heap mode (InlineCapacity > 0)
// ============================================================

TEST(WTF_SegmentedVector, InlineEmpty)
{
    SegmentedVector<int, 4, 4> vector;
    EXPECT_TRUE(vector.isEmpty());
    EXPECT_EQ(0u, vector.size());
}

TEST(WTF_SegmentedVector, InlineAppendAndAccess)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(1);
    vector.append(2);
    vector.append(3);

    EXPECT_FALSE(vector.isEmpty());
    EXPECT_EQ(3u, vector.size());

    EXPECT_EQ(1, vector.at(0));
    EXPECT_EQ(2, vector.at(1));
    EXPECT_EQ(3, vector.at(2));

    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
}

TEST(WTF_SegmentedVector, InlineFirstAndLast)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(10);
    vector.append(20);
    vector.append(30);

    EXPECT_EQ(10, vector.first());
    EXPECT_EQ(30, vector.last());
}

TEST(WTF_SegmentedVector, InlineOnlyStorage)
{
    // Fill exactly to InlineCapacity — no heap allocation should occur
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 4; ++i)
        vector.append(i * 10);

    EXPECT_EQ(4u, vector.size());
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(i * 10, vector[i]);
}

TEST(WTF_SegmentedVector, InlineToHeapTransition)
{
    // Add InlineCapacity+1 elements to trigger the first heap allocation
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 5; ++i)
        vector.append(i * 10);

    EXPECT_EQ(5u, vector.size());
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(i * 10, vector[i]);
}

TEST(WTF_SegmentedVector, InlineHeapMultipleSegments)
{
    // Test that elements beyond inline storage go to heap segments
    SegmentedVector<int, 4, 4> vector;

    // 12 elements: 4 inline + 2 heap segments of 4
    for (int i = 0; i < 12; ++i)
        vector.append(i);

    EXPECT_EQ(12u, vector.size());
    for (int i = 0; i < 12; ++i)
        EXPECT_EQ(i, vector[i]);
}

TEST(WTF_SegmentedVector, InlinePointerStability)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(100);
    int* firstPtr = &vector[0];

    // Add more elements, triggering heap segment allocation
    for (int i = 1; i < 20; ++i)
        vector.append(i * 100);

    // Original pointer should still be valid
    EXPECT_EQ(100, *firstPtr);

    // Also verify pointers to heap elements remain stable
    int* heapPtr = &vector[10];
    for (int i = 20; i < 40; ++i)
        vector.append(i * 100);

    EXPECT_EQ(1000, *heapPtr);
}

TEST(WTF_SegmentedVector, InlinePointerStabilityAtBoundary)
{
    SegmentedVector<int, 4, 4> vector;

    // Fill inline storage
    for (int i = 0; i < 4; ++i)
        vector.append(i);

    // Pointer to last inline element
    int* lastInlinePtr = &vector[3];

    // Add one more element — first heap element
    vector.append(99);
    int* firstHeapPtr = &vector[4];

    // Add more elements to grow further
    for (int i = 5; i < 20; ++i)
        vector.append(i);

    // Both pointers should remain valid
    EXPECT_EQ(3, *lastInlinePtr);
    EXPECT_EQ(99, *firstHeapPtr);
}

TEST(WTF_SegmentedVector, InlineRemoveLast)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(1);
    vector.append(2);
    vector.append(3);

    EXPECT_EQ(3u, vector.size());

    vector.removeLast();
    EXPECT_EQ(2u, vector.size());
    EXPECT_EQ(2, vector.last());

    vector.removeLast();
    EXPECT_EQ(1u, vector.size());
    EXPECT_EQ(1, vector.last());

    vector.removeLast();
    EXPECT_TRUE(vector.isEmpty());
}

TEST(WTF_SegmentedVector, InlineRemoveLastAcrossBoundary)
{
    SegmentedVector<int, 4, 4> vector;

    // Fill past inline into heap
    for (int i = 0; i < 6; ++i)
        vector.append(i * 10);

    EXPECT_EQ(6u, vector.size());

    // Remove back into inline range
    vector.removeLast(); // removes 50
    vector.removeLast(); // removes 40
    EXPECT_EQ(4u, vector.size());
    EXPECT_EQ(30, vector.last());

    // Remove within inline range
    vector.removeLast(); // removes 30
    EXPECT_EQ(3u, vector.size());
    EXPECT_EQ(20, vector.last());
}

TEST(WTF_SegmentedVector, InlineTakeLast)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(10);
    vector.append(20);
    vector.append(30);

    EXPECT_EQ(30, vector.takeLast());
    EXPECT_EQ(2u, vector.size());

    EXPECT_EQ(20, vector.takeLast());
    EXPECT_EQ(1u, vector.size());
}

TEST(WTF_SegmentedVector, InlineClear)
{
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 20; ++i)
        vector.append(i);

    EXPECT_EQ(20u, vector.size());

    vector.clear();
    EXPECT_TRUE(vector.isEmpty());
    EXPECT_EQ(0u, vector.size());
}

TEST(WTF_SegmentedVector, InlineClearAndReuse)
{
    SegmentedVector<int, 4, 4> vector;

    // Fill past inline into heap
    for (int i = 0; i < 10; ++i)
        vector.append(i);

    vector.clear();
    EXPECT_TRUE(vector.isEmpty());

    // Re-append — should reuse inline storage
    for (int i = 0; i < 3; ++i)
        vector.append(i * 100);

    EXPECT_EQ(3u, vector.size());
    EXPECT_EQ(0, vector[0]);
    EXPECT_EQ(100, vector[1]);
    EXPECT_EQ(200, vector[2]);
}

TEST(WTF_SegmentedVector, InlineGrowWithinInline)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(42);
    vector.grow(4); // grow to exactly InlineCapacity

    EXPECT_EQ(4u, vector.size());
    EXPECT_EQ(42, vector[0]);
    for (size_t i = 1; i < 4; ++i)
        EXPECT_EQ(0, vector[i]);
}

TEST(WTF_SegmentedVector, InlineGrowAcrossBoundary)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(42);
    vector.append(43);
    vector.grow(7); // from 2 (inline) to 7 (inline + heap)

    EXPECT_EQ(7u, vector.size());
    EXPECT_EQ(42, vector[0]);
    EXPECT_EQ(43, vector[1]);
    for (size_t i = 2; i < 7; ++i)
        EXPECT_EQ(0, vector[i]);
}

TEST(WTF_SegmentedVector, InlineGrow)
{
    SegmentedVector<int, 4, 4> vector;

    vector.append(1);
    vector.grow(10);

    EXPECT_EQ(10u, vector.size());
    EXPECT_EQ(1, vector[0]);

    // Default-constructed elements should be 0
    for (size_t i = 1; i < 10; ++i)
        EXPECT_EQ(0, vector[i]);
}

TEST(WTF_SegmentedVector, InlineShrinkToFit)
{
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 20; ++i)
        vector.append(i);

    // shrinkToFit should not affect functionality
    vector.shrinkToFit();

    EXPECT_EQ(20u, vector.size());
    for (int i = 0; i < 20; ++i)
        EXPECT_EQ(i, vector[i]);
}

TEST(WTF_SegmentedVector, InlineIteratorInlineOnly)
{
    // Iterator over exactly InlineCapacity elements (no heap)
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 4; ++i)
        vector.append(i * 10);

    int expected = 0;
    for (auto& value : vector) {
        EXPECT_EQ(expected, value);
        expected += 10;
    }
    EXPECT_EQ(40, expected);
}

TEST(WTF_SegmentedVector, InlineIteratorSpanningSegments)
{
    SegmentedVector<int, 4, 4> vector;

    for (int i = 0; i < 12; ++i)
        vector.append(i * 10);

    int expected = 0;
    for (auto& value : vector) {
        EXPECT_EQ(expected, value);
        expected += 10;
    }
    EXPECT_EQ(120, expected);
}

TEST(WTF_SegmentedVector, InlineMoveOnly)
{
    SegmentedVector<MoveOnly, 4, 4> vector;

    vector.append(MoveOnly(1));
    vector.append(MoveOnly(2));
    vector.append(MoveOnly(3));

    EXPECT_EQ(1u, vector[0].value());
    EXPECT_EQ(2u, vector[1].value());
    EXPECT_EQ(3u, vector[2].value());

    auto last = vector.takeLast();
    EXPECT_EQ(3u, last.value());
    EXPECT_EQ(2u, vector.size());
}

TEST(WTF_SegmentedVector, InlineMoveOnlySpanningSegments)
{
    SegmentedVector<MoveOnly, 4, 4> vector;

    for (unsigned i = 0; i < 12; ++i)
        vector.append(MoveOnly(i));

    EXPECT_EQ(12u, vector.size());

    for (unsigned i = 0; i < 12; ++i)
        EXPECT_EQ(i, vector[i].value());
}

TEST(WTF_SegmentedVector, InlineAlloc)
{
    SegmentedVector<int, 4, 4> vector;

    int& ref1 = vector.alloc(100);
    EXPECT_EQ(100, ref1);
    EXPECT_EQ(1u, vector.size());

    int& ref2 = vector.alloc(200);
    EXPECT_EQ(200, ref2);
    EXPECT_EQ(2u, vector.size());

    // References should remain valid
    EXPECT_EQ(100, ref1);
    EXPECT_EQ(200, ref2);
}

TEST(WTF_SegmentedVector, InlineAllocSpanningSegments)
{
    SegmentedVector<int, 4, 4> vector;

    // Alloc elements across inline and heap segments
    int* refs[12];
    for (int i = 0; i < 12; ++i)
        refs[i] = &vector.alloc(i * 100);

    // All references should remain valid
    for (int i = 0; i < 12; ++i)
        EXPECT_EQ(i * 100, *refs[i]);
}

TEST(WTF_SegmentedVector, InlineConstructAndAppend)
{
    SegmentedVector<int, 4, 4> vector;

    vector.constructAndAppend(42);
    vector.constructAndAppend(43);

    EXPECT_EQ(2u, vector.size());
    EXPECT_EQ(42, vector[0]);
    EXPECT_EQ(43, vector[1]);
}

TEST(WTF_SegmentedVector, InlineDifferentSegmentSizes)
{
    // Test with different inline and heap segment sizes
    SegmentedVector<int, 8, 2> vector;

    for (int i = 0; i < 20; ++i)
        vector.append(i);

    EXPECT_EQ(20u, vector.size());
    for (int i = 0; i < 20; ++i)
        EXPECT_EQ(i, vector[i]);
}

TEST(WTF_SegmentedVector, InlineLargeInlineSegment)
{
    // Test with larger inline segment
    SegmentedVector<int, 4, 16> vector;

    for (int i = 0; i < 32; ++i)
        vector.append(i);

    EXPECT_EQ(32u, vector.size());
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(i, vector[i]);
}

// ============================================================
// Section 3: Destructor verification
// ============================================================

TEST(WTF_SegmentedVector, DestructorCalledOnRemoveLast)
{
    SegmentedVector<DestructorCounter, 4> vector;

    vector.append(DestructorCounter(1));
    vector.append(DestructorCounter(2));
    vector.append(DestructorCounter(3));

    DestructorCounter::resetCount();

    vector.removeLast();
    EXPECT_EQ(1u, DestructorCounter::s_destructorCount);

    vector.removeLast();
    EXPECT_EQ(2u, DestructorCounter::s_destructorCount);
}

TEST(WTF_SegmentedVector, DestructorCalledOnClear)
{
    SegmentedVector<DestructorCounter, 4> vector;

    vector.append(DestructorCounter(1));
    vector.append(DestructorCounter(2));
    vector.append(DestructorCounter(3));

    DestructorCounter::resetCount();

    vector.clear();
    EXPECT_EQ(3u, DestructorCounter::s_destructorCount);
}

TEST(WTF_SegmentedVector, DestructorCalledOnDestruction)
{
    {
        SegmentedVector<DestructorCounter, 4> vector;

        vector.append(DestructorCounter(1));
        vector.append(DestructorCounter(2));
        vector.append(DestructorCounter(3));

        // Reset count after construction
        DestructorCounter::resetCount();
    }

    // Vector destructor should have destroyed all 3 elements
    EXPECT_EQ(3u, DestructorCounter::s_destructorCount);
}

} // namespace TestWebKitAPI
